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
#include "scopes.h"
#include "usage-analyzer.h"

namespace v8 {
namespace internal {

// Weight boundaries
static const int MinWeight = 1;
static const int MaxWeight = 1000000;
static const int InitialWeight = 100;


class UsageComputer: public AstVisitor {
 public:
  static bool Traverse(AstNode* node);

  // AST node visit functions.
#define DECLARE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  void VisitVariable(Variable* var);

 private:
  int weight_;
  bool is_write_;

  UsageComputer(int weight, bool is_write);
  virtual ~UsageComputer();

  // Helper functions
  void RecordUses(UseCount* uses);
  void Read(Expression* x);
  void Write(Expression* x);
  void ReadList(ZoneList<Expression*>* list);
  void ReadList(ZoneList<ObjectLiteral::Property*>* list);

  friend class WeightScaler;
};


class WeightScaler BASE_EMBEDDED {
 public:
  WeightScaler(UsageComputer* uc, float scale);
  ~WeightScaler();

 private:
  UsageComputer* uc_;
  int old_weight_;
};


// ----------------------------------------------------------------------------
// Implementation of UsageComputer

bool UsageComputer::Traverse(AstNode* node) {
  UsageComputer uc(InitialWeight, false);
  uc.Visit(node);
  return !uc.HasStackOverflow();
}


void UsageComputer::VisitBlock(Block* node) {
  VisitStatements(node->statements());
}


void UsageComputer::VisitDeclaration(Declaration* node) {
  Write(node->proxy());
  if (node->fun() != NULL)
    VisitFunctionLiteral(node->fun());
}


void UsageComputer::VisitExpressionStatement(ExpressionStatement* node) {
  Visit(node->expression());
}


void UsageComputer::VisitEmptyStatement(EmptyStatement* node) {
  // nothing to do
}


void UsageComputer::VisitIfStatement(IfStatement* node) {
  Read(node->condition());
  { WeightScaler ws(this, 0.5);  // executed 50% of the time
    Visit(node->then_statement());
    Visit(node->else_statement());
  }
}


void UsageComputer::VisitContinueStatement(ContinueStatement* node) {
  // nothing to do
}


void UsageComputer::VisitBreakStatement(BreakStatement* node) {
  // nothing to do
}


void UsageComputer::VisitReturnStatement(ReturnStatement* node) {
  Read(node->expression());
}


void UsageComputer::VisitWithEnterStatement(WithEnterStatement* node) {
  Read(node->expression());
}


void UsageComputer::VisitWithExitStatement(WithExitStatement* node) {
  // nothing to do
}


void UsageComputer::VisitSwitchStatement(SwitchStatement* node) {
  Read(node->tag());
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = cases->length(); i-- > 0;) {
    WeightScaler ws(this, static_cast<float>(1.0 / cases->length()));
    CaseClause* clause = cases->at(i);
    if (!clause->is_default())
      Read(clause->label());
    VisitStatements(clause->statements());
  }
}


void UsageComputer::VisitDoWhileStatement(DoWhileStatement* node) {
  WeightScaler ws(this, 10.0);
  Read(node->cond());
  Visit(node->body());
}


void UsageComputer::VisitWhileStatement(WhileStatement* node) {
  WeightScaler ws(this, 10.0);
  Read(node->cond());
  Visit(node->body());
}


void UsageComputer::VisitForStatement(ForStatement* node) {
  if (node->init() != NULL) Visit(node->init());
  { WeightScaler ws(this, 10.0);  // executed in each iteration
    if (node->cond() != NULL) Read(node->cond());
    if (node->next() != NULL) Visit(node->next());
    Visit(node->body());
  }
}


void UsageComputer::VisitForInStatement(ForInStatement* node) {
  WeightScaler ws(this, 10.0);
  Write(node->each());
  Read(node->enumerable());
  Visit(node->body());
}


void UsageComputer::VisitTryCatchStatement(TryCatchStatement* node) {
  Visit(node->try_block());
  { WeightScaler ws(this, 0.25);
    Write(node->catch_var());
    Visit(node->catch_block());
  }
}


void UsageComputer::VisitTryFinallyStatement(TryFinallyStatement* node) {
  Visit(node->try_block());
  Visit(node->finally_block());
}


void UsageComputer::VisitDebuggerStatement(DebuggerStatement* node) {
}


void UsageComputer::VisitFunctionLiteral(FunctionLiteral* node) {
  ZoneList<Declaration*>* decls = node->scope()->declarations();
  for (int i = 0; i < decls->length(); i++) VisitDeclaration(decls->at(i));
  VisitStatements(node->body());
}


void UsageComputer::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  // Do nothing.
}


void UsageComputer::VisitConditional(Conditional* node) {
  Read(node->condition());
  { WeightScaler ws(this, 0.5);
    Read(node->then_expression());
    Read(node->else_expression());
  }
}


void UsageComputer::VisitSlot(Slot* node) {
  UNREACHABLE();
}


void UsageComputer::VisitVariable(Variable* node) {
  RecordUses(node->var_uses());
}


void UsageComputer::VisitVariableProxy(VariableProxy* node) {
  // The proxy may refer to a variable in which case it was bound via
  // VariableProxy::BindTo.
  RecordUses(node->var_uses());
}


void UsageComputer::VisitLiteral(Literal* node) {
  // nothing to do
}

void UsageComputer::VisitRegExpLiteral(RegExpLiteral* node) {
  // nothing to do
}


void UsageComputer::VisitObjectLiteral(ObjectLiteral* node) {
  ReadList(node->properties());
}


void UsageComputer::VisitArrayLiteral(ArrayLiteral* node) {
  ReadList(node->values());
}


void UsageComputer::VisitCatchExtensionObject(CatchExtensionObject* node) {
  Read(node->value());
}


void UsageComputer::VisitAssignment(Assignment* node) {
  if (node->op() != Token::ASSIGN)
    Read(node->target());
  Write(node->target());
  Read(node->value());
}


void UsageComputer::VisitThrow(Throw* node) {
  Read(node->exception());
}


void UsageComputer::VisitProperty(Property* node) {
  // In any case (read or write) we read both the
  // node's object and the key.
  Read(node->obj());
  Read(node->key());
  // If the node's object is a variable proxy,
  // we have a 'simple' object property access. We count
  // the access via the variable or proxy's object uses.
  VariableProxy* proxy = node->obj()->AsVariableProxy();
  if (proxy != NULL) {
    RecordUses(proxy->obj_uses());
  }
}


void UsageComputer::VisitCall(Call* node) {
  Read(node->expression());
  ReadList(node->arguments());
}


void UsageComputer::VisitCallNew(CallNew* node) {
  Read(node->expression());
  ReadList(node->arguments());
}


void UsageComputer::VisitCallRuntime(CallRuntime* node) {
  ReadList(node->arguments());
}


void UsageComputer::VisitUnaryOperation(UnaryOperation* node) {
  Read(node->expression());
}


void UsageComputer::VisitCountOperation(CountOperation* node) {
  Read(node->expression());
  Write(node->expression());
}


void UsageComputer::VisitBinaryOperation(BinaryOperation* node) {
  Read(node->left());
  Read(node->right());
}


void UsageComputer::VisitCompareOperation(CompareOperation* node) {
  Read(node->left());
  Read(node->right());
}


void UsageComputer::VisitThisFunction(ThisFunction* node) {
}


UsageComputer::UsageComputer(int weight, bool is_write) {
  weight_ = weight;
  is_write_ = is_write;
}


UsageComputer::~UsageComputer() {
  // nothing to do
}


void UsageComputer::RecordUses(UseCount* uses) {
  if (is_write_)
    uses->RecordWrite(weight_);
  else
    uses->RecordRead(weight_);
}


void UsageComputer::Read(Expression* x) {
  if (is_write_) {
    UsageComputer uc(weight_, false);
    uc.Visit(x);
  } else {
    Visit(x);
  }
}


void UsageComputer::Write(Expression* x) {
  if (!is_write_) {
    UsageComputer uc(weight_, true);
    uc.Visit(x);
  } else {
    Visit(x);
  }
}


void UsageComputer::ReadList(ZoneList<Expression*>* list) {
  for (int i = list->length(); i-- > 0; )
    Read(list->at(i));
}


void UsageComputer::ReadList(ZoneList<ObjectLiteral::Property*>* list) {
  for (int i = list->length(); i-- > 0; )
    Read(list->at(i)->value());
}


// ----------------------------------------------------------------------------
// Implementation of WeightScaler

WeightScaler::WeightScaler(UsageComputer* uc, float scale) {
  uc_ = uc;
  old_weight_ = uc->weight_;
  int new_weight = static_cast<int>(uc->weight_ * scale);
  if (new_weight <= 0) new_weight = MinWeight;
  else if (new_weight > MaxWeight) new_weight = MaxWeight;
  uc->weight_ = new_weight;
}


WeightScaler::~WeightScaler() {
  uc_->weight_ = old_weight_;
}


// ----------------------------------------------------------------------------
// Interface to variable usage analysis

bool AnalyzeVariableUsage(FunctionLiteral* lit) {
  if (!FLAG_usage_computation) return true;
  HistogramTimerScope timer(&Counters::usage_analysis);
  return UsageComputer::Traverse(lit);
}

} }  // namespace v8::internal
