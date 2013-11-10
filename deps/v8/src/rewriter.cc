// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "rewriter.h"

#include "ast.h"
#include "compiler.h"
#include "scopes.h"

namespace v8 {
namespace internal {

class Processor: public AstVisitor {
 public:
  Processor(Variable* result, Zone* zone)
      : result_(result),
        result_assigned_(false),
        is_set_(false),
        in_try_(false),
        factory_(zone->isolate(), zone) {
    InitializeAstVisitor(zone->isolate());
  }

  virtual ~Processor() { }

  void Process(ZoneList<Statement*>* statements);
  bool result_assigned() const { return result_assigned_; }

  AstNodeFactory<AstNullVisitor>* factory() {
    return &factory_;
  }

 private:
  Variable* result_;

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

  AstNodeFactory<AstNullVisitor> factory_;

  Expression* SetResult(Expression* value) {
    result_assigned_ = true;
    VariableProxy* result_proxy = factory()->NewVariableProxy(result_);
    return factory()->NewAssignment(
        Token::ASSIGN, result_proxy, value, RelocInfo::kNoPosition);
  }

  // Node visitors.
#define DEF_VISIT(type) \
  virtual void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  void VisitIterationStatement(IterationStatement* stmt);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
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


void Processor::VisitModuleStatement(ModuleStatement* node) {
  bool set_after_body = is_set_;
  Visit(node->body());
  is_set_ = is_set_ && set_after_body;
}


void Processor::VisitExpressionStatement(ExpressionStatement* node) {
  // Rewrite : <x>; -> .result = <x>;
  if (!is_set_ && !node->expression()->IsThrow()) {
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


void Processor::VisitForOfStatement(ForOfStatement* node) {
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


void Processor::VisitCaseClause(CaseClause* clause) {
  UNREACHABLE();
}


void Processor::VisitContinueStatement(ContinueStatement* node) {
  is_set_ = false;
}


void Processor::VisitBreakStatement(BreakStatement* node) {
  is_set_ = false;
}


void Processor::VisitWithStatement(WithStatement* node) {
  bool set_after_body = is_set_;
  Visit(node->statement());
  is_set_ = is_set_ && set_after_body;
}


// Do nothing:
void Processor::VisitVariableDeclaration(VariableDeclaration* node) {}
void Processor::VisitFunctionDeclaration(FunctionDeclaration* node) {}
void Processor::VisitModuleDeclaration(ModuleDeclaration* node) {}
void Processor::VisitImportDeclaration(ImportDeclaration* node) {}
void Processor::VisitExportDeclaration(ExportDeclaration* node) {}
void Processor::VisitModuleLiteral(ModuleLiteral* node) {}
void Processor::VisitModuleVariable(ModuleVariable* node) {}
void Processor::VisitModulePath(ModulePath* node) {}
void Processor::VisitModuleUrl(ModuleUrl* node) {}
void Processor::VisitEmptyStatement(EmptyStatement* node) {}
void Processor::VisitReturnStatement(ReturnStatement* node) {}
void Processor::VisitDebuggerStatement(DebuggerStatement* node) {}


// Expressions are never visited yet.
#define DEF_VISIT(type)                                         \
  void Processor::Visit##type(type* expr) { UNREACHABLE(); }
EXPRESSION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT


// Assumes code has been parsed.  Mutates the AST, so the AST should not
// continue to be used in the case of failure.
bool Rewriter::Rewrite(CompilationInfo* info) {
  FunctionLiteral* function = info->function();
  ASSERT(function != NULL);
  Scope* scope = function->scope();
  ASSERT(scope != NULL);
  if (!scope->is_global_scope() && !scope->is_eval_scope()) return true;

  ZoneList<Statement*>* body = function->body();
  if (!body->is_empty()) {
    Variable* result = scope->NewTemporary(
        info->isolate()->factory()->result_string());
    Processor processor(result, info->zone());
    processor.Process(body);
    if (processor.HasStackOverflow()) return false;

    if (processor.result_assigned()) {
      ASSERT(function->end_position() != RelocInfo::kNoPosition);
      // Set the position of the assignment statement one character past the
      // source code, such that it definitely is not in the source code range
      // of an immediate inner scope. For example in
      //   eval('with ({x:1}) x = 1');
      // the end position of the function generated for executing the eval code
      // coincides with the end of the with scope which is the position of '1'.
      int pos = function->end_position();
      VariableProxy* result_proxy = processor.factory()->NewVariableProxy(
          result->name(), false, result->interface(), pos);
      result_proxy->BindTo(result);
      Statement* result_statement =
          processor.factory()->NewReturnStatement(result_proxy, pos);
      body->Add(result_statement, info->zone());
    }
  }

  return true;
}


} }  // namespace v8::internal
