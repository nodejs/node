// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/rewriter.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {

class Processor: public AstVisitor {
 public:
  Processor(Isolate* isolate, Scope* scope, Variable* result,
            AstValueFactory* ast_value_factory)
      : result_(result),
        result_assigned_(false),
        replacement_(nullptr),
        is_set_(false),
        zone_(ast_value_factory->zone()),
        scope_(scope),
        factory_(ast_value_factory) {
    InitializeAstVisitor(isolate);
  }

  Processor(Parser* parser, Scope* scope, Variable* result,
            AstValueFactory* ast_value_factory)
      : result_(result),
        result_assigned_(false),
        replacement_(nullptr),
        is_set_(false),
        zone_(ast_value_factory->zone()),
        scope_(scope),
        factory_(ast_value_factory) {
    InitializeAstVisitor(parser->stack_limit());
  }

  ~Processor() override {}

  void Process(ZoneList<Statement*>* statements);
  bool result_assigned() const { return result_assigned_; }

  Zone* zone() { return zone_; }
  Scope* scope() { return scope_; }
  AstNodeFactory* factory() { return &factory_; }

  // Returns ".result = value"
  Expression* SetResult(Expression* value) {
    result_assigned_ = true;
    VariableProxy* result_proxy = factory()->NewVariableProxy(result_);
    return factory()->NewAssignment(Token::ASSIGN, result_proxy, value,
                                    RelocInfo::kNoPosition);
  }

  // Inserts '.result = undefined' in front of the given statement.
  Statement* AssignUndefinedBefore(Statement* s);

 private:
  Variable* result_;

  // We are not tracking result usage via the result_'s use
  // counts (we leave the accurate computation to the
  // usage analyzer). Instead we simple remember if
  // there was ever an assignment to result_.
  bool result_assigned_;

  // When visiting a node, we "return" a replacement for that node in
  // [replacement_].  In many cases this will just be the original node.
  Statement* replacement_;

  // To avoid storing to .result all the time, we eliminate some of
  // the stores by keeping track of whether or not we're sure .result
  // will be overwritten anyway. This is a bit more tricky than what I
  // was hoping for.
  bool is_set_;

  Zone* zone_;
  Scope* scope_;
  AstNodeFactory factory_;

  // Node visitors.
#define DEF_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  void VisitIterationStatement(IterationStatement* stmt);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
};


Statement* Processor::AssignUndefinedBefore(Statement* s) {
  Expression* result_proxy = factory()->NewVariableProxy(result_);
  Expression* undef = factory()->NewUndefinedLiteral(RelocInfo::kNoPosition);
  Expression* assignment = factory()->NewAssignment(
      Token::ASSIGN, result_proxy, undef, RelocInfo::kNoPosition);
  Block* b = factory()->NewBlock(NULL, 2, false, RelocInfo::kNoPosition);
  b->statements()->Add(
      factory()->NewExpressionStatement(assignment, RelocInfo::kNoPosition),
      zone());
  b->statements()->Add(s, zone());
  return b;
}


void Processor::Process(ZoneList<Statement*>* statements) {
  for (int i = statements->length() - 1; i >= 0; --i) {
    Visit(statements->at(i));
    statements->Set(i, replacement_);
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
  if (!node->ignore_completion_value()) Process(node->statements());
  replacement_ = node;
}


void Processor::VisitExpressionStatement(ExpressionStatement* node) {
  // Rewrite : <x>; -> .result = <x>;
  if (!is_set_) {
    node->set_expression(SetResult(node->expression()));
    is_set_ = true;
  }
  replacement_ = node;
}


void Processor::VisitIfStatement(IfStatement* node) {
  // Rewrite both branches.
  bool set_after = is_set_;
  Visit(node->then_statement());
  node->set_then_statement(replacement_);
  bool set_in_then = is_set_;
  is_set_ = set_after;
  Visit(node->else_statement());
  node->set_else_statement(replacement_);
  is_set_ = is_set_ && set_in_then;
  replacement_ = node;

  if (!is_set_) {
    is_set_ = true;
    replacement_ = AssignUndefinedBefore(node);
  }
}


void Processor::VisitIterationStatement(IterationStatement* node) {
  // Rewrite the body.
  bool set_after = is_set_;
  is_set_ = false;  // We are in a loop, so we can't rely on [set_after].
  Visit(node->body());
  node->set_body(replacement_);
  is_set_ = is_set_ && set_after;
  replacement_ = node;

  if (!is_set_) {
    is_set_ = true;
    replacement_ = AssignUndefinedBefore(node);
  }
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


void Processor::VisitForOfStatement(ForOfStatement* node) {
  VisitIterationStatement(node);
}


void Processor::VisitTryCatchStatement(TryCatchStatement* node) {
  // Rewrite both try and catch block.
  bool set_after = is_set_;
  Visit(node->try_block());
  node->set_try_block(static_cast<Block*>(replacement_));
  bool set_in_try = is_set_;
  is_set_ = set_after;
  Visit(node->catch_block());
  node->set_catch_block(static_cast<Block*>(replacement_));
  is_set_ = is_set_ && set_in_try;
  replacement_ = node;

  if (!is_set_) {
    is_set_ = true;
    replacement_ = AssignUndefinedBefore(node);
  }
}


void Processor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  // Rewrite both try and finally block (in reverse order).
  bool set_after = is_set_;
  is_set_ = true;  // Don't normally need to assign in finally block.
  Visit(node->finally_block());
  node->set_finally_block(replacement_->AsBlock());
  {  // Save .result value at the beginning of the finally block and restore it
     // at the end again: ".backup = .result; ...; .result = .backup"
     // This is necessary because the finally block does not normally contribute
     // to the completion value.
    CHECK(scope() != nullptr);
    Variable* backup = scope()->NewTemporary(
        factory()->ast_value_factory()->dot_result_string());
    Expression* backup_proxy = factory()->NewVariableProxy(backup);
    Expression* result_proxy = factory()->NewVariableProxy(result_);
    Expression* save = factory()->NewAssignment(
        Token::ASSIGN, backup_proxy, result_proxy, RelocInfo::kNoPosition);
    Expression* restore = factory()->NewAssignment(
        Token::ASSIGN, result_proxy, backup_proxy, RelocInfo::kNoPosition);
    node->finally_block()->statements()->InsertAt(
        0, factory()->NewExpressionStatement(save, RelocInfo::kNoPosition),
        zone());
    node->finally_block()->statements()->Add(
        factory()->NewExpressionStatement(restore, RelocInfo::kNoPosition),
        zone());
  }
  is_set_ = set_after;
  Visit(node->try_block());
  node->set_try_block(replacement_->AsBlock());
  replacement_ = node;

  if (!is_set_) {
    is_set_ = true;
    replacement_ = AssignUndefinedBefore(node);
  }
}


void Processor::VisitSwitchStatement(SwitchStatement* node) {
  // Rewrite statements in all case clauses (in reverse order).
  ZoneList<CaseClause*>* clauses = node->cases();
  bool set_after = is_set_;
  for (int i = clauses->length() - 1; i >= 0; --i) {
    CaseClause* clause = clauses->at(i);
    Process(clause->statements());
  }
  is_set_ = is_set_ && set_after;
  replacement_ = node;

  if (!is_set_) {
    is_set_ = true;
    replacement_ = AssignUndefinedBefore(node);
  }
}


void Processor::VisitContinueStatement(ContinueStatement* node) {
  is_set_ = false;
  replacement_ = node;
}


void Processor::VisitBreakStatement(BreakStatement* node) {
  is_set_ = false;
  replacement_ = node;
}


void Processor::VisitWithStatement(WithStatement* node) {
  Visit(node->statement());
  node->set_statement(replacement_);
  replacement_ = node;

  if (!is_set_) {
    is_set_ = true;
    replacement_ = AssignUndefinedBefore(node);
  }
}


void Processor::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  Visit(node->statement());
  node->set_statement(replacement_);
  replacement_ = node;
}


void Processor::VisitEmptyStatement(EmptyStatement* node) {
  replacement_ = node;
}


void Processor::VisitReturnStatement(ReturnStatement* node) {
  is_set_ = true;
  replacement_ = node;
}


void Processor::VisitDebuggerStatement(DebuggerStatement* node) {
  replacement_ = node;
}


// Expressions are never visited.
#define DEF_VISIT(type)                                         \
  void Processor::Visit##type(type* expr) { UNREACHABLE(); }
EXPRESSION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT


// Declarations are never visited.
#define DEF_VISIT(type) \
  void Processor::Visit##type(type* expr) { UNREACHABLE(); }
DECLARATION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT


// Assumes code has been parsed.  Mutates the AST, so the AST should not
// continue to be used in the case of failure.
bool Rewriter::Rewrite(ParseInfo* info) {
  FunctionLiteral* function = info->literal();
  DCHECK(function != NULL);
  Scope* scope = function->scope();
  DCHECK(scope != NULL);
  if (!scope->is_script_scope() && !scope->is_eval_scope()) return true;

  ZoneList<Statement*>* body = function->body();
  if (!body->is_empty()) {
    Variable* result =
        scope->NewTemporary(info->ast_value_factory()->dot_result_string());
    // The name string must be internalized at this point.
    DCHECK(!result->name().is_null());
    Processor processor(info->isolate(), scope, result,
                        info->ast_value_factory());
    processor.Process(body);
    if (processor.HasStackOverflow()) return false;

    if (processor.result_assigned()) {
      DCHECK(function->end_position() != RelocInfo::kNoPosition);
      // Set the position of the assignment statement one character past the
      // source code, such that it definitely is not in the source code range
      // of an immediate inner scope. For example in
      //   eval('with ({x:1}) x = 1');
      // the end position of the function generated for executing the eval code
      // coincides with the end of the with scope which is the position of '1'.
      int pos = function->end_position();
      VariableProxy* result_proxy =
          processor.factory()->NewVariableProxy(result, pos);
      Statement* result_statement =
          processor.factory()->NewReturnStatement(result_proxy, pos);
      body->Add(result_statement, info->zone());
    }
  }

  return true;
}


bool Rewriter::Rewrite(Parser* parser, DoExpression* expr,
                       AstValueFactory* factory) {
  Block* block = expr->block();
  Scope* scope = block->scope();
  ZoneList<Statement*>* body = block->statements();
  VariableProxy* result = expr->result();
  Variable* result_var = result->var();

  if (!body->is_empty()) {
    Processor processor(parser, scope, result_var, factory);
    processor.Process(body);
    if (processor.HasStackOverflow()) return false;

    if (!processor.result_assigned()) {
      AstNodeFactory* node_factory = processor.factory();
      Expression* undef =
          node_factory->NewUndefinedLiteral(RelocInfo::kNoPosition);
      Statement* completion = node_factory->NewExpressionStatement(
          processor.SetResult(undef), expr->position());
      body->Add(completion, factory->zone());
    }
  }
  return true;
}


}  // namespace internal
}  // namespace v8
