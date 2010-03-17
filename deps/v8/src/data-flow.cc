// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "data-flow.h"
#include "scopes.h"

namespace v8 {
namespace internal {


#ifdef DEBUG
void BitVector::Print() {
  bool first = true;
  PrintF("{");
  for (int i = 0; i < length(); i++) {
    if (Contains(i)) {
      if (!first) PrintF(",");
      first = false;
      PrintF("%d");
    }
  }
  PrintF("}");
}
#endif


void FlowGraph::AppendInstruction(AstNode* instruction) {
  // Add a (non-null) AstNode to the end of the graph fragment.
  ASSERT(instruction != NULL);
  if (exit()->IsExitNode()) return;
  if (!exit()->IsBlockNode()) AppendNode(new BlockNode());
  BlockNode::cast(exit())->AddInstruction(instruction);
}


void FlowGraph::AppendNode(Node* node) {
  // Add a node to the end of the graph.  An empty block is added to
  // maintain edge-split form (that no join nodes or exit nodes as
  // successors to branch nodes).
  ASSERT(node != NULL);
  if (exit()->IsExitNode()) return;
  if (exit()->IsBranchNode() && (node->IsJoinNode() || node->IsExitNode())) {
    AppendNode(new BlockNode());
  }
  exit()->AddSuccessor(node);
  node->AddPredecessor(exit());
  exit_ = node;
}


void FlowGraph::AppendGraph(FlowGraph* graph) {
  // Add a flow graph fragment to the end of this one.  An empty block is
  // added to maintain edge-split form (that no join nodes or exit nodes as
  // successors to branch nodes).
  ASSERT(graph != NULL);
  if (exit()->IsExitNode()) return;
  Node* node = graph->entry();
  if (exit()->IsBranchNode() && (node->IsJoinNode() || node->IsExitNode())) {
    AppendNode(new BlockNode());
  }
  exit()->AddSuccessor(node);
  node->AddPredecessor(exit());
  exit_ = graph->exit();
}


void FlowGraph::Split(BranchNode* branch,
                      FlowGraph* left,
                      FlowGraph* right,
                      JoinNode* join) {
  // Add the branch node, left flowgraph, join node.
  AppendNode(branch);
  AppendGraph(left);
  AppendNode(join);

  // Splice in the right flowgraph.
  right->AppendNode(join);
  branch->AddSuccessor(right->entry());
  right->entry()->AddPredecessor(branch);
}


void FlowGraph::Loop(JoinNode* join,
                     FlowGraph* condition,
                     BranchNode* branch,
                     FlowGraph* body) {
  // Add the join, condition and branch.  Add join's predecessors in
  // left-to-right order.
  AppendNode(join);
  body->AppendNode(join);
  AppendGraph(condition);
  AppendNode(branch);

  // Splice in the body flowgraph.
  branch->AddSuccessor(body->entry());
  body->entry()->AddPredecessor(branch);
}


void ExitNode::Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder) {
  preorder->Add(this);
  postorder->Add(this);
}


void BlockNode::Traverse(bool mark,
                         ZoneList<Node*>* preorder,
                         ZoneList<Node*>* postorder) {
  ASSERT(successor_ != NULL);
  preorder->Add(this);
  if (!successor_->IsMarkedWith(mark)) {
    successor_->MarkWith(mark);
    successor_->Traverse(mark, preorder, postorder);
  }
  postorder->Add(this);
}


void BranchNode::Traverse(bool mark,
                          ZoneList<Node*>* preorder,
                          ZoneList<Node*>* postorder) {
  ASSERT(successor0_ != NULL && successor1_ != NULL);
  preorder->Add(this);
  if (!successor1_->IsMarkedWith(mark)) {
    successor1_->MarkWith(mark);
    successor1_->Traverse(mark, preorder, postorder);
  }
  if (!successor0_->IsMarkedWith(mark)) {
    successor0_->MarkWith(mark);
    successor0_->Traverse(mark, preorder, postorder);
  }
  postorder->Add(this);
}


void JoinNode::Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder) {
  ASSERT(successor_ != NULL);
  preorder->Add(this);
  if (!successor_->IsMarkedWith(mark)) {
    successor_->MarkWith(mark);
    successor_->Traverse(mark, preorder, postorder);
  }
  postorder->Add(this);
}


void FlowGraphBuilder::Build(FunctionLiteral* lit) {
  global_exit_ = new ExitNode();
  VisitStatements(lit->body());

  if (HasStackOverflow()) return;

  // The graph can end with a branch node (if the function ended with a
  // loop).  Maintain edge-split form (no join nodes or exit nodes as
  // successors to branch nodes).
  if (graph_.exit()->IsBranchNode()) graph_.AppendNode(new BlockNode());
  graph_.AppendNode(global_exit_);

  // Build preorder and postorder traversal orders.  All the nodes in
  // the graph have the same mark flag.  For the traversal, use that
  // flag's negation.  Traversal will flip all the flags.
  bool mark = graph_.entry()->IsMarkedWith(false);
  graph_.entry()->MarkWith(mark);
  graph_.entry()->Traverse(mark, &preorder_, &postorder_);
}


void FlowGraphBuilder::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void FlowGraphBuilder::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void FlowGraphBuilder::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void FlowGraphBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  // Nothing to do.
}


void FlowGraphBuilder::VisitIfStatement(IfStatement* stmt) {
  Visit(stmt->condition());

  BranchNode* branch = new BranchNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->then_statement());

  FlowGraph left = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->else_statement());

  if (HasStackOverflow()) return;
  JoinNode* join = new JoinNode();
  original.Split(branch, &left, &graph_, join);
  graph_ = original;
}


void FlowGraphBuilder::VisitContinueStatement(ContinueStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitBreakStatement(BreakStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitWithEnterStatement(WithEnterStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitWithExitStatement(WithExitStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitWhileStatement(WhileStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != NULL) Visit(stmt->init());

  JoinNode* join = new JoinNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  if (stmt->cond() != NULL) Visit(stmt->cond());

  BranchNode* branch = new BranchNode();
  FlowGraph condition = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->body());

  if (stmt->next() != NULL) Visit(stmt->next());

  if (HasStackOverflow()) return;
  original.Loop(join, &condition, branch, &graph_);
  graph_ = original;
}


void FlowGraphBuilder::VisitForInStatement(ForInStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitConditional(Conditional* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void FlowGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitLiteral(Literal* expr) {
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitAssignment(Assignment* expr) {
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  // Left-hand side can be a variable or property (or reference error) but
  // not both.
  ASSERT(var == NULL || prop == NULL);
  if (var != NULL) {
    if (expr->is_compound()) Visit(expr->target());
    Visit(expr->value());
    if (var->IsStackAllocated()) {
      expr->set_num(definitions_.length());
      definitions_.Add(expr);
    }

  } else if (prop != NULL) {
    Visit(prop->obj());
    if (!prop->key()->IsPropertyName()) Visit(prop->key());
    Visit(expr->value());
  }

  if (HasStackOverflow()) return;
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitThrow(Throw* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitProperty(Property* expr) {
  Visit(expr->obj());
  if (!expr->key()->IsPropertyName()) Visit(expr->key());

  if (HasStackOverflow()) return;
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCall(Call* expr) {
  Visit(expr->expression());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    Visit(arguments->at(i));
  }

  if (HasStackOverflow()) return;
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCallNew(CallNew* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::NOT:
    case Token::BIT_NOT:
    case Token::DELETE:
    case Token::TYPEOF:
    case Token::VOID:
      SetStackOverflow();
      break;

    case Token::ADD:
    case Token::SUB:
      Visit(expr->expression());
      if (HasStackOverflow()) return;
      graph_.AppendInstruction(expr);
      break;

    default:
      UNREACHABLE();
  }
}


void FlowGraphBuilder::VisitCountOperation(CountOperation* expr) {
  Visit(expr->expression());
  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  if (var != NULL && var->IsStackAllocated()) {
    expr->set_num(definitions_.length());
    definitions_.Add(expr);
  }

  if (HasStackOverflow()) return;
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA:
    case Token::OR:
    case Token::AND:
      SetStackOverflow();
      break;

    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::SHL:
    case Token::SHR:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
    case Token::SAR:
      Visit(expr->left());
      Visit(expr->right());
      if (HasStackOverflow()) return;
      graph_.AppendInstruction(expr);
      break;

    default:
      UNREACHABLE();
  }
}


void FlowGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  switch (expr->op()) {
    case Token::EQ:
    case Token::NE:
    case Token::EQ_STRICT:
    case Token::NE_STRICT:
    case Token::INSTANCEOF:
    case Token::IN:
      SetStackOverflow();
      break;

    case Token::LT:
    case Token::GT:
    case Token::LTE:
    case Token::GTE:
      Visit(expr->left());
      Visit(expr->right());
      if (HasStackOverflow()) return;
      graph_.AppendInstruction(expr);
      break;

    default:
      UNREACHABLE();
  }
}


void FlowGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  SetStackOverflow();
}


void AstLabeler::Label(CompilationInfo* info) {
  info_ = info;
  VisitStatements(info_->function()->body());
}


void AstLabeler::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
  }
}


void AstLabeler::VisitDeclarations(ZoneList<Declaration*>* decls) {
  UNREACHABLE();
}


void AstLabeler::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void AstLabeler::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void AstLabeler::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void AstLabeler::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitReturnStatement(ReturnStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  UNREACHABLE();
}


void AstLabeler::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitVariableProxy(VariableProxy* expr) {
  expr->set_num(next_number_++);
  Variable* var = expr->var();
  if (var->is_global() && !var->is_this()) {
    info_->set_has_globals(true);
  }
}


void AstLabeler::VisitLiteral(Literal* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->IsPropertyName());
  VariableProxy* proxy = prop->obj()->AsVariableProxy();
  USE(proxy);
  ASSERT(proxy != NULL && proxy->var()->is_this());
  info()->set_has_this_properties(true);

  prop->obj()->set_num(AstNode::kNoNumber);
  prop->key()->set_num(AstNode::kNoNumber);
  Visit(expr->value());
  expr->set_num(next_number_++);
}


void AstLabeler::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitProperty(Property* expr) {
  ASSERT(expr->key()->IsPropertyName());
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  USE(proxy);
  ASSERT(proxy != NULL && proxy->var()->is_this());
  info()->set_has_this_properties(true);

  expr->obj()->set_num(AstNode::kNoNumber);
  expr->key()->set_num(AstNode::kNoNumber);
  expr->set_num(next_number_++);
}


void AstLabeler::VisitCall(Call* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitBinaryOperation(BinaryOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
  expr->set_num(next_number_++);
}


void AstLabeler::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


void AstLabeler::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


ZoneList<Expression*>* VarUseMap::Lookup(Variable* var) {
  HashMap::Entry* entry = HashMap::Lookup(var, var->name()->Hash(), true);
  if (entry->value == NULL) {
    entry->value = new ZoneList<Expression*>(1);
  }
  return reinterpret_cast<ZoneList<Expression*>*>(entry->value);
}


void LivenessAnalyzer::Analyze(FunctionLiteral* fun) {
  // Process the function body.
  VisitStatements(fun->body());

  // All variables are implicitly defined at the function start.
  // Record a definition of all variables live at function entry.
  for (HashMap::Entry* p = live_vars_.Start();
       p != NULL;
       p = live_vars_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->key);
    RecordDef(var, fun);
  }
}


void LivenessAnalyzer::VisitStatements(ZoneList<Statement*>* stmts) {
  // Visit statements right-to-left.
  for (int i = stmts->length() - 1; i >= 0; i--) {
    Visit(stmts->at(i));
  }
}


void LivenessAnalyzer::RecordUse(Variable* var, Expression* expr) {
  ASSERT(var->is_global() || var->is_this());
  ZoneList<Expression*>* uses = live_vars_.Lookup(var);
  uses->Add(expr);
}


void LivenessAnalyzer::RecordDef(Variable* var, Expression* expr) {
  ASSERT(var->is_global() || var->is_this());

  // We do not support other expressions that can define variables.
  ASSERT(expr->AsFunctionLiteral() != NULL);

  // Add the variable to the list of defined variables.
  if (expr->defined_vars() == NULL) {
    expr->set_defined_vars(new ZoneList<DefinitionInfo*>(1));
  }
  DefinitionInfo* def = new DefinitionInfo();
  expr->AsFunctionLiteral()->defined_vars()->Add(def);

  // Compute the last use of the definition. The variable uses are
  // inserted in reversed evaluation order. The first element
  // in the list of live uses is the last use.
  ZoneList<Expression*>* uses = live_vars_.Lookup(var);
  while (uses->length() > 0) {
    Expression* use_site = uses->RemoveLast();
    use_site->set_var_def(def);
    if (uses->length() == 0) {
      def->set_last_use(use_site);
    }
  }
}


// Visitor functions for live variable analysis.
void LivenessAnalyzer::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void LivenessAnalyzer::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void LivenessAnalyzer::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void LivenessAnalyzer::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitReturnStatement(ReturnStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->var();
  ASSERT(var->is_global());
  ASSERT(!var->is_this());
  RecordUse(var, expr);
}


void LivenessAnalyzer::VisitLiteral(Literal* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitAssignment(Assignment* expr) {
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->IsPropertyName());
  VariableProxy* proxy = prop->obj()->AsVariableProxy();
  ASSERT(proxy != NULL && proxy->var()->is_this());

  // Record use of this at the assignment node. Assignments to
  // this-properties are treated like unary operations.
  RecordUse(proxy->var(), expr);

  // Visit right-hand side.
  Visit(expr->value());
}


void LivenessAnalyzer::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitProperty(Property* expr) {
  ASSERT(expr->key()->IsPropertyName());
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  ASSERT(proxy != NULL && proxy->var()->is_this());
  RecordUse(proxy->var(), expr);
}


void LivenessAnalyzer::VisitCall(Call* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitBinaryOperation(BinaryOperation* expr) {
  // Visit child nodes in reverse evaluation order.
  Visit(expr->right());
  Visit(expr->left());
}


void LivenessAnalyzer::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void LivenessAnalyzer::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


AssignedVariablesAnalyzer::AssignedVariablesAnalyzer(FunctionLiteral* fun)
    : fun_(fun),
      av_(fun->scope()->num_parameters() + fun->scope()->num_stack_slots()) {}


void AssignedVariablesAnalyzer::Analyze() {
  ASSERT(av_.length() > 0);
  VisitStatements(fun_->body());
}


Variable* AssignedVariablesAnalyzer::FindSmiLoopVariable(ForStatement* stmt) {
  // The loop must have all necessary parts.
  if (stmt->init() == NULL || stmt->cond() == NULL || stmt->next() == NULL) {
    return NULL;
  }
  // The initialization statement has to be a simple assignment.
  Assignment* init = stmt->init()->StatementAsSimpleAssignment();
  if (init == NULL) return NULL;

  // We only deal with local variables.
  Variable* loop_var = init->target()->AsVariableProxy()->AsVariable();
  if (loop_var == NULL || !loop_var->IsStackAllocated()) return NULL;

  // The initial value has to be a smi.
  Literal* init_lit = init->value()->AsLiteral();
  if (init_lit == NULL || !init_lit->handle()->IsSmi()) return NULL;
  int init_value = Smi::cast(*init_lit->handle())->value();

  // The condition must be a compare of variable with <, <=, >, or >=.
  CompareOperation* cond = stmt->cond()->AsCompareOperation();
  if (cond == NULL) return NULL;
  if (cond->op() != Token::LT
      && cond->op() != Token::LTE
      && cond->op() != Token::GT
      && cond->op() != Token::GTE) return NULL;

  // The lhs must be the same variable as in the init expression.
  if (cond->left()->AsVariableProxy()->AsVariable() != loop_var) return NULL;

  // The rhs must be a smi.
  Literal* term_lit = cond->right()->AsLiteral();
  if (term_lit == NULL || !term_lit->handle()->IsSmi()) return NULL;
  int term_value = Smi::cast(*term_lit->handle())->value();

  // The count operation updates the same variable as in the init expression.
  CountOperation* update = stmt->next()->StatementAsCountOperation();
  if (update == NULL) return NULL;
  if (update->expression()->AsVariableProxy()->AsVariable() != loop_var) {
    return NULL;
  }

  // The direction of the count operation must agree with the start and the end
  // value. We currently do not allow the initial value to be the same as the
  // terminal value. This _would_ be ok as long as the loop body never executes
  // or executes exactly one time.
  if (init_value == term_value) return NULL;
  if (init_value < term_value && update->op() != Token::INC) return NULL;
  if (init_value > term_value && update->op() != Token::DEC) return NULL;

  // Check that the update operation cannot overflow the smi range. This can
  // occur in the two cases where the loop bound is equal to the largest or
  // smallest smi.
  if (update->op() == Token::INC && term_value == Smi::kMaxValue) return NULL;
  if (update->op() == Token::DEC && term_value == Smi::kMinValue) return NULL;

  // Found a smi loop variable.
  return loop_var;
}

int AssignedVariablesAnalyzer::BitIndex(Variable* var) {
  ASSERT(var != NULL);
  ASSERT(var->IsStackAllocated());
  Slot* slot = var->slot();
  if (slot->type() == Slot::PARAMETER) {
    return slot->index();
  } else {
    return fun_->scope()->num_parameters() + slot->index();
  }
}


void AssignedVariablesAnalyzer::RecordAssignedVar(Variable* var) {
  ASSERT(var != NULL);
  if (var->IsStackAllocated()) {
    av_.Add(BitIndex(var));
  }
}


void AssignedVariablesAnalyzer::MarkIfTrivial(Expression* expr) {
  Variable* var = expr->AsVariableProxy()->AsVariable();
  if (var != NULL &&
      var->IsStackAllocated() &&
      !var->is_arguments() &&
      var->mode() != Variable::CONST &&
      (var->is_this() || !av_.Contains(BitIndex(var)))) {
    expr->AsVariableProxy()->set_is_trivial(true);
  }
}


void AssignedVariablesAnalyzer::ProcessExpression(Expression* expr) {
  BitVector saved_av(av_);
  av_.Clear();
  Visit(expr);
  av_.Union(saved_av);
}

void AssignedVariablesAnalyzer::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void AssignedVariablesAnalyzer::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  ProcessExpression(stmt->expression());
}


void AssignedVariablesAnalyzer::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void AssignedVariablesAnalyzer::VisitIfStatement(IfStatement* stmt) {
  ProcessExpression(stmt->condition());
  Visit(stmt->then_statement());
  Visit(stmt->else_statement());
}


void AssignedVariablesAnalyzer::VisitContinueStatement(
    ContinueStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitBreakStatement(BreakStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitReturnStatement(ReturnStatement* stmt) {
  ProcessExpression(stmt->expression());
}


void AssignedVariablesAnalyzer::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  ProcessExpression(stmt->expression());
}


void AssignedVariablesAnalyzer::VisitWithExitStatement(
    WithExitStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitSwitchStatement(SwitchStatement* stmt) {
  BitVector result(av_);
  av_.Clear();
  Visit(stmt->tag());
  result.Union(av_);
  for (int i = 0; i < stmt->cases()->length(); i++) {
    CaseClause* clause = stmt->cases()->at(i);
    if (!clause->is_default()) {
      av_.Clear();
      Visit(clause->label());
      result.Union(av_);
    }
    VisitStatements(clause->statements());
  }
  av_.Union(result);
}


void AssignedVariablesAnalyzer::VisitDoWhileStatement(DoWhileStatement* stmt) {
  ProcessExpression(stmt->cond());
  Visit(stmt->body());
}


void AssignedVariablesAnalyzer::VisitWhileStatement(WhileStatement* stmt) {
  ProcessExpression(stmt->cond());
  Visit(stmt->body());
}


void AssignedVariablesAnalyzer::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != NULL) Visit(stmt->init());

  if (stmt->cond() != NULL) ProcessExpression(stmt->cond());

  if (stmt->next() != NULL) Visit(stmt->next());

  // Process loop body. After visiting the loop body av_ contains
  // the assigned variables of the loop body.
  BitVector saved_av(av_);
  av_.Clear();
  Visit(stmt->body());

  Variable* var = FindSmiLoopVariable(stmt);
  if (var != NULL && !av_.Contains(BitIndex(var))) {
    stmt->set_loop_variable(var);
  }

  av_.Union(saved_av);
}


void AssignedVariablesAnalyzer::VisitForInStatement(ForInStatement* stmt) {
  ProcessExpression(stmt->each());
  ProcessExpression(stmt->enumerable());
  Visit(stmt->body());
}


void AssignedVariablesAnalyzer::VisitTryCatchStatement(
    TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->catch_block());
}


void AssignedVariablesAnalyzer::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  Visit(stmt->try_block());
  Visit(stmt->finally_block());
}


void AssignedVariablesAnalyzer::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  // Nothing to do.
}


void AssignedVariablesAnalyzer::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitConditional(Conditional* expr) {
  ASSERT(av_.IsEmpty());

  Visit(expr->condition());

  BitVector result(av_);
  av_.Clear();
  Visit(expr->then_expression());
  result.Union(av_);

  av_.Clear();
  Visit(expr->else_expression());
  av_.Union(result);
}


void AssignedVariablesAnalyzer::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void AssignedVariablesAnalyzer::VisitVariableProxy(VariableProxy* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitLiteral(Literal* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitRegExpLiteral(RegExpLiteral* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitObjectLiteral(ObjectLiteral* expr) {
  ASSERT(av_.IsEmpty());
  BitVector result(av_.length());
  for (int i = 0; i < expr->properties()->length(); i++) {
    Visit(expr->properties()->at(i)->value());
    result.Union(av_);
    av_.Clear();
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitArrayLiteral(ArrayLiteral* expr) {
  ASSERT(av_.IsEmpty());
  BitVector result(av_.length());
  for (int i = 0; i < expr->values()->length(); i++) {
    Visit(expr->values()->at(i));
    result.Union(av_);
    av_.Clear();
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->key());
  ProcessExpression(expr->value());
}


void AssignedVariablesAnalyzer::VisitAssignment(Assignment* expr) {
  ASSERT(av_.IsEmpty());

  if (expr->target()->AsProperty() != NULL) {
    // Visit receiver and key of property store and rhs.
    Visit(expr->target()->AsProperty()->obj());
    ProcessExpression(expr->target()->AsProperty()->key());
    ProcessExpression(expr->value());

    // If we have a variable as a receiver in a property store, check if
    // we can mark it as trivial.
    MarkIfTrivial(expr->target()->AsProperty()->obj());
  } else {
    Visit(expr->target());
    ProcessExpression(expr->value());

    Variable* var = expr->target()->AsVariableProxy()->AsVariable();
    if (var != NULL) RecordAssignedVar(var);
  }
}


void AssignedVariablesAnalyzer::VisitThrow(Throw* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->exception());
}


void AssignedVariablesAnalyzer::VisitProperty(Property* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->obj());
  ProcessExpression(expr->key());

  // In case we have a variable as a receiver, check if we can mark
  // it as trivial.
  MarkIfTrivial(expr->obj());
}


void AssignedVariablesAnalyzer::VisitCall(Call* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->expression());
  BitVector result(av_);
  for (int i = 0; i < expr->arguments()->length(); i++) {
    av_.Clear();
    Visit(expr->arguments()->at(i));
    result.Union(av_);
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitCallNew(CallNew* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->expression());
  BitVector result(av_);
  for (int i = 0; i < expr->arguments()->length(); i++) {
    av_.Clear();
    Visit(expr->arguments()->at(i));
    result.Union(av_);
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitCallRuntime(CallRuntime* expr) {
  ASSERT(av_.IsEmpty());
  BitVector result(av_);
  for (int i = 0; i < expr->arguments()->length(); i++) {
    av_.Clear();
    Visit(expr->arguments()->at(i));
    result.Union(av_);
  }
  av_ = result;
}


void AssignedVariablesAnalyzer::VisitUnaryOperation(UnaryOperation* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->expression());
}


void AssignedVariablesAnalyzer::VisitCountOperation(CountOperation* expr) {
  ASSERT(av_.IsEmpty());

  Visit(expr->expression());

  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  if (var != NULL) RecordAssignedVar(var);
}


void AssignedVariablesAnalyzer::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->left());

  ProcessExpression(expr->right());

  // In case we have a variable on the left side, check if we can mark
  // it as trivial.
  MarkIfTrivial(expr->left());
}


void AssignedVariablesAnalyzer::VisitCompareOperation(CompareOperation* expr) {
  ASSERT(av_.IsEmpty());
  Visit(expr->left());

  ProcessExpression(expr->right());

  // In case we have a variable on the left side, check if we can mark
  // it as trivial.
  MarkIfTrivial(expr->left());
}


void AssignedVariablesAnalyzer::VisitThisFunction(ThisFunction* expr) {
  // Nothing to do.
  ASSERT(av_.IsEmpty());
}


void AssignedVariablesAnalyzer::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


#ifdef DEBUG

// Print a textual representation of an instruction in a flow graph.  Using
// the AstVisitor is overkill because there is no recursion here.  It is
// only used for printing in debug mode.
class TextInstructionPrinter: public AstVisitor {
 public:
  TextInstructionPrinter() : number_(0) {}

  int NextNumber() { return number_; }
  void AssignNumber(AstNode* node) { node->set_num(number_++); }

 private:
  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  int number_;

  DISALLOW_COPY_AND_ASSIGN(TextInstructionPrinter);
};


void TextInstructionPrinter::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitBlock(Block* stmt) {
  PrintF("Block");
}


void TextInstructionPrinter::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  PrintF("ExpressionStatement");
}


void TextInstructionPrinter::VisitEmptyStatement(EmptyStatement* stmt) {
  PrintF("EmptyStatement");
}


void TextInstructionPrinter::VisitIfStatement(IfStatement* stmt) {
  PrintF("IfStatement");
}


void TextInstructionPrinter::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitReturnStatement(ReturnStatement* stmt) {
  PrintF("return @%d", stmt->expression()->num());
}


void TextInstructionPrinter::VisitWithEnterStatement(WithEnterStatement* stmt) {
  PrintF("WithEnterStatement");
}


void TextInstructionPrinter::VisitWithExitStatement(WithExitStatement* stmt) {
  PrintF("WithExitStatement");
}


void TextInstructionPrinter::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitDoWhileStatement(DoWhileStatement* stmt) {
  PrintF("DoWhileStatement");
}


void TextInstructionPrinter::VisitWhileStatement(WhileStatement* stmt) {
  PrintF("WhileStatement");
}


void TextInstructionPrinter::VisitForStatement(ForStatement* stmt) {
  PrintF("ForStatement");
}


void TextInstructionPrinter::VisitForInStatement(ForInStatement* stmt) {
  PrintF("ForInStatement");
}


void TextInstructionPrinter::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitDebuggerStatement(DebuggerStatement* stmt) {
  PrintF("DebuggerStatement");
}


void TextInstructionPrinter::VisitFunctionLiteral(FunctionLiteral* expr) {
  PrintF("FunctionLiteral");
}


void TextInstructionPrinter::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  PrintF("FunctionBoilerplateLiteral");
}


void TextInstructionPrinter::VisitConditional(Conditional* expr) {
  PrintF("Conditional");
}


void TextInstructionPrinter::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void TextInstructionPrinter::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->AsVariable();
  if (var != NULL) {
    PrintF("%s", *var->name()->ToCString());
    if (var->IsStackAllocated() && expr->reaching_definitions() != NULL) {
      expr->reaching_definitions()->Print();
    }
  } else {
    ASSERT(expr->AsProperty() != NULL);
    VisitProperty(expr->AsProperty());
  }
}


void TextInstructionPrinter::VisitLiteral(Literal* expr) {
  expr->handle()->ShortPrint();
}


void TextInstructionPrinter::VisitRegExpLiteral(RegExpLiteral* expr) {
  PrintF("RegExpLiteral");
}


void TextInstructionPrinter::VisitObjectLiteral(ObjectLiteral* expr) {
  PrintF("ObjectLiteral");
}


void TextInstructionPrinter::VisitArrayLiteral(ArrayLiteral* expr) {
  PrintF("ArrayLiteral");
}


void TextInstructionPrinter::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  PrintF("CatchExtensionObject");
}


void TextInstructionPrinter::VisitAssignment(Assignment* expr) {
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();

  if (var == NULL && prop == NULL) {
    // Throw reference error.
    Visit(expr->target());
    return;
  }

  // Print the left-hand side.
  if (var != NULL) {
    PrintF("%s", *var->name()->ToCString());
  } else if (prop != NULL) {
    PrintF("@%d", prop->obj()->num());
    if (prop->key()->IsPropertyName()) {
      PrintF(".");
      ASSERT(prop->key()->AsLiteral() != NULL);
      prop->key()->AsLiteral()->handle()->Print();
    } else {
      PrintF("[@%d]", prop->key()->num());
    }
  }

  // Print the operation.
  if (expr->is_compound()) {
    PrintF(" = ");
    // Print the left-hand side again when compound.
    if (var != NULL) {
      PrintF("@%d", expr->target()->num());
    } else {
      PrintF("@%d", prop->obj()->num());
      if (prop->key()->IsPropertyName()) {
        PrintF(".");
        ASSERT(prop->key()->AsLiteral() != NULL);
        prop->key()->AsLiteral()->handle()->Print();
      } else {
        PrintF("[@%d]", prop->key()->num());
      }
    }
    // Print the corresponding binary operator.
    PrintF(" %s ", Token::String(expr->binary_op()));
  } else {
    PrintF(" %s ", Token::String(expr->op()));
  }

  // Print the right-hand side.
  PrintF("@%d", expr->value()->num());

  if (expr->num() != AstNode::kNoNumber) {
    PrintF(" ;; D%d", expr->num());
  }
}


void TextInstructionPrinter::VisitThrow(Throw* expr) {
  PrintF("throw @%d", expr->exception()->num());
}


void TextInstructionPrinter::VisitProperty(Property* expr) {
  if (expr->key()->IsPropertyName()) {
    PrintF("@%d.", expr->obj()->num());
    ASSERT(expr->key()->AsLiteral() != NULL);
    expr->key()->AsLiteral()->handle()->Print();
  } else {
    PrintF("@%d[@%d]", expr->obj()->num(), expr->key()->num());
  }
}


void TextInstructionPrinter::VisitCall(Call* expr) {
  PrintF("@%d(", expr->expression()->num());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", arguments->at(i)->num());
  }
  PrintF(")");
}


void TextInstructionPrinter::VisitCallNew(CallNew* expr) {
  PrintF("new @%d(", expr->expression()->num());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", arguments->at(i)->num());
  }
  PrintF(")");
}


void TextInstructionPrinter::VisitCallRuntime(CallRuntime* expr) {
  PrintF("%s(", *expr->name()->ToCString());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", arguments->at(i)->num());
  }
  PrintF(")");
}


void TextInstructionPrinter::VisitUnaryOperation(UnaryOperation* expr) {
  PrintF("%s(@%d)", Token::String(expr->op()), expr->expression()->num());
}


void TextInstructionPrinter::VisitCountOperation(CountOperation* expr) {
  if (expr->is_prefix()) {
    PrintF("%s@%d", Token::String(expr->op()), expr->expression()->num());
  } else {
    PrintF("@%d%s", expr->expression()->num(), Token::String(expr->op()));
  }

  if (expr->num() != AstNode::kNoNumber) {
    PrintF(" ;; D%d", expr->num());
  }
}


void TextInstructionPrinter::VisitBinaryOperation(BinaryOperation* expr) {
  ASSERT(expr->op() != Token::COMMA);
  ASSERT(expr->op() != Token::OR);
  ASSERT(expr->op() != Token::AND);
  PrintF("@%d %s @%d",
         expr->left()->num(),
         Token::String(expr->op()),
         expr->right()->num());
}


void TextInstructionPrinter::VisitCompareOperation(CompareOperation* expr) {
  PrintF("@%d %s @%d",
         expr->left()->num(),
         Token::String(expr->op()),
         expr->right()->num());
}


void TextInstructionPrinter::VisitThisFunction(ThisFunction* expr) {
  PrintF("ThisFunction");
}


static int node_count = 0;
static int instruction_count = 0;


void Node::AssignNodeNumber() {
  set_number(node_count++);
}


void Node::PrintReachingDefinitions() {
  if (rd_.rd_in() != NULL) {
    ASSERT(rd_.kill() != NULL && rd_.gen() != NULL);

    PrintF("RD_in = ");
    rd_.rd_in()->Print();
    PrintF("\n");

    PrintF("RD_kill = ");
    rd_.kill()->Print();
    PrintF("\n");

    PrintF("RD_gen = ");
    rd_.gen()->Print();
    PrintF("\n");
  }
}


void ExitNode::PrintText() {
  PrintReachingDefinitions();
  PrintF("L%d: Exit\n\n", number());
}


void BlockNode::PrintText() {
  PrintReachingDefinitions();
  // Print the instructions in the block.
  PrintF("L%d: Block\n", number());
  TextInstructionPrinter printer;
  for (int i = 0, len = instructions_.length(); i < len; i++) {
    PrintF("%d ", printer.NextNumber());
    printer.Visit(instructions_[i]);
    printer.AssignNumber(instructions_[i]);
    PrintF("\n");
  }
  PrintF("goto L%d\n\n", successor_->number());
}


void BranchNode::PrintText() {
  PrintReachingDefinitions();
  PrintF("L%d: Branch\n", number());
  PrintF("goto (L%d, L%d)\n\n", successor0_->number(), successor1_->number());
}


void JoinNode::PrintText() {
  PrintReachingDefinitions();
  PrintF("L%d: Join(", number());
  for (int i = 0, len = predecessors_.length(); i < len; i++) {
    if (i != 0) PrintF(", ");
    PrintF("L%d", predecessors_[i]->number());
  }
  PrintF(")\ngoto L%d\n\n", successor_->number());
}


void FlowGraph::PrintText(ZoneList<Node*>* postorder) {
  PrintF("\n========\n");

  // Number nodes and instructions in reverse postorder.
  node_count = 0;
  instruction_count = 0;
  for (int i = postorder->length() - 1; i >= 0; i--) {
    postorder->at(i)->AssignNodeNumber();
  }

  // Print basic blocks in reverse postorder.
  for (int i = postorder->length() - 1; i >= 0; i--) {
    postorder->at(i)->PrintText();
  }
}


#endif  // defined(DEBUG)


int ReachingDefinitions::IndexFor(Variable* var, int variable_count) {
  // Parameters are numbered left-to-right from the beginning of the bit
  // set.  Stack-allocated locals are allocated right-to-left from the end.
  ASSERT(var != NULL && var->IsStackAllocated());
  Slot* slot = var->slot();
  if (slot->type() == Slot::PARAMETER) {
    return slot->index();
  } else {
    return (variable_count - 1) - slot->index();
  }
}


void Node::InitializeReachingDefinitions(int definition_count,
                                         List<BitVector*>* variables,
                                         WorkList<Node>* worklist,
                                         bool mark) {
  ASSERT(!IsMarkedWith(mark));
  rd_.Initialize(definition_count);
  MarkWith(mark);
  worklist->Insert(this);
}


void BlockNode::InitializeReachingDefinitions(int definition_count,
                                              List<BitVector*>* variables,
                                              WorkList<Node>* worklist,
                                              bool mark) {
  ASSERT(!IsMarkedWith(mark));
  int instruction_count = instructions_.length();
  int variable_count = variables->length();

  rd_.Initialize(definition_count);

  for (int i = 0; i < instruction_count; i++) {
    Expression* expr = instructions_[i]->AsExpression();
    if (expr == NULL) continue;
    Variable* var = expr->AssignedVar();
    if (var == NULL || !var->IsStackAllocated()) continue;

    // All definitions of this variable are killed.
    BitVector* def_set =
        variables->at(ReachingDefinitions::IndexFor(var, variable_count));
    rd_.kill()->Union(*def_set);

    // All previously generated definitions are not generated.
    rd_.gen()->Subtract(*def_set);

    // This one is generated.
    rd_.gen()->Add(expr->num());
  }

  // Add all blocks except the entry node to the worklist.
  if (predecessor_ != NULL) {
    MarkWith(mark);
    worklist->Insert(this);
  }
}


void ExitNode::ComputeRDOut(BitVector* result) {
  // Should not be the predecessor of any node.
  UNREACHABLE();
}


void BlockNode::ComputeRDOut(BitVector* result) {
  // All definitions reaching this block ...
  *result = *rd_.rd_in();
  // ... except those killed by the block ...
  result->Subtract(*rd_.kill());
  // ... but including those generated by the block.
  result->Union(*rd_.gen());
}


void BranchNode::ComputeRDOut(BitVector* result) {
  // Branch nodes don't kill or generate definitions.
  *result = *rd_.rd_in();
}


void JoinNode::ComputeRDOut(BitVector* result) {
  // Join nodes don't kill or generate definitions.
  *result = *rd_.rd_in();
}


void ExitNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  // The exit node has no successors so we can just update in place.  New
  // RD_in is the union over all predecessors.
  int definition_count = rd_.rd_in()->length();
  rd_.rd_in()->Clear();

  BitVector temp(definition_count);
  for (int i = 0, len = predecessors_.length(); i < len; i++) {
    // Because ComputeRDOut always overwrites temp and its value is
    // always read out before calling ComputeRDOut again, we do not
    // have to clear it on each iteration of the loop.
    predecessors_[i]->ComputeRDOut(&temp);
    rd_.rd_in()->Union(temp);
  }
}


void BlockNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  // The entry block has no predecessor.  Its RD_in does not change.
  if (predecessor_ == NULL) return;

  BitVector new_rd_in(rd_.rd_in()->length());
  predecessor_->ComputeRDOut(&new_rd_in);

  if (rd_.rd_in()->Equals(new_rd_in)) return;

  // Update RD_in.
  *rd_.rd_in() = new_rd_in;
  // Add the successor to the worklist if not already present.
  if (!successor_->IsMarkedWith(mark)) {
    successor_->MarkWith(mark);
    worklist->Insert(successor_);
  }
}


void BranchNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  BitVector new_rd_in(rd_.rd_in()->length());
  predecessor_->ComputeRDOut(&new_rd_in);

  if (rd_.rd_in()->Equals(new_rd_in)) return;

  // Update RD_in.
  *rd_.rd_in() = new_rd_in;
  // Add the successors to the worklist if not already present.
  if (!successor0_->IsMarkedWith(mark)) {
    successor0_->MarkWith(mark);
    worklist->Insert(successor0_);
  }
  if (!successor1_->IsMarkedWith(mark)) {
    successor1_->MarkWith(mark);
    worklist->Insert(successor1_);
  }
}


void JoinNode::UpdateRDIn(WorkList<Node>* worklist, bool mark) {
  int definition_count = rd_.rd_in()->length();
  BitVector new_rd_in(definition_count);

  // New RD_in is the union over all predecessors.
  BitVector temp(definition_count);
  for (int i = 0, len = predecessors_.length(); i < len; i++) {
    predecessors_[i]->ComputeRDOut(&temp);
    new_rd_in.Union(temp);
  }

  if (rd_.rd_in()->Equals(new_rd_in)) return;

  // Update RD_in.
  *rd_.rd_in() = new_rd_in;
  // Add the successor to the worklist if not already present.
  if (!successor_->IsMarkedWith(mark)) {
    successor_->MarkWith(mark);
    worklist->Insert(successor_);
  }
}


void Node::PropagateReachingDefinitions(List<BitVector*>* variables) {
  // Nothing to do.
}


void BlockNode::PropagateReachingDefinitions(List<BitVector*>* variables) {
  // Propagate RD_in from the start of the block to all the variable
  // references.
  int variable_count = variables->length();
  BitVector rd = *rd_.rd_in();
  for (int i = 0, len = instructions_.length(); i < len; i++) {
    Expression* expr = instructions_[i]->AsExpression();
    if (expr == NULL) continue;

    // Look for a variable reference to record its reaching definitions.
    VariableProxy* proxy = expr->AsVariableProxy();
    if (proxy == NULL) {
      // Not a VariableProxy?  Maybe it's a count operation.
      CountOperation* count_operation = expr->AsCountOperation();
      if (count_operation != NULL) {
        proxy = count_operation->expression()->AsVariableProxy();
      }
    }
    if (proxy == NULL) {
      // OK, Maybe it's a compound assignment.
      Assignment* assignment = expr->AsAssignment();
      if (assignment != NULL && assignment->is_compound()) {
        proxy = assignment->target()->AsVariableProxy();
      }
    }

    if (proxy != NULL &&
        proxy->var()->IsStackAllocated() &&
        !proxy->var()->is_this()) {
      // All definitions for this variable.
      BitVector* definitions =
          variables->at(ReachingDefinitions::IndexFor(proxy->var(),
                                                      variable_count));
      BitVector* reaching_definitions = new BitVector(*definitions);
      // Intersected with all definitions (of any variable) reaching this
      // instruction.
      reaching_definitions->Intersect(rd);
      proxy->set_reaching_definitions(reaching_definitions);
    }

    // It may instead (or also) be a definition.  If so update the running
    // value of reaching definitions for the block.
    Variable* var = expr->AssignedVar();
    if (var == NULL || !var->IsStackAllocated()) continue;

    // All definitions of this variable are killed.
    BitVector* def_set =
        variables->at(ReachingDefinitions::IndexFor(var, variable_count));
    rd.Subtract(*def_set);
    // This definition is generated.
    rd.Add(expr->num());
  }
}


void ReachingDefinitions::Compute() {
  ASSERT(!definitions_->is_empty());

  int variable_count = variables_.length();
  int definition_count = definitions_->length();
  int node_count = postorder_->length();

  // Step 1: For each variable, identify the set of all its definitions in
  // the body.
  for (int i = 0; i < definition_count; i++) {
    Variable* var = definitions_->at(i)->AssignedVar();
    variables_[IndexFor(var, variable_count)]->Add(i);
  }

  if (FLAG_print_graph_text) {
    for (int i = 0; i < variable_count; i++) {
      BitVector* def_set = variables_[i];
      if (!def_set->IsEmpty()) {
        // At least one definition.
        bool first = true;
        for (int j = 0; j < definition_count; j++) {
          if (def_set->Contains(j)) {
            if (first) {
              Variable* var = definitions_->at(j)->AssignedVar();
              ASSERT(var != NULL);
              PrintF("Def[%s] = {%d", *var->name()->ToCString(), j);
              first = false;
            } else {
              PrintF(",%d", j);
            }
          }
        }
        PrintF("}\n");
      }
    }
  }

  // Step 2: Compute KILL and GEN for each block node, initialize RD_in for
  // all nodes, and mark and add all nodes to the worklist in reverse
  // postorder.  All nodes should currently have the same mark.
  bool mark = postorder_->at(0)->IsMarkedWith(false);  // Negation of current.
  WorkList<Node> worklist(node_count);
  for (int i = node_count - 1; i >= 0; i--) {
    postorder_->at(i)->InitializeReachingDefinitions(definition_count,
                                                     &variables_,
                                                     &worklist,
                                                     mark);
  }

  // Step 3: Until the worklist is empty, remove an item compute and update
  // its rd_in based on its predecessor's rd_out.  If rd_in has changed, add
  // all necessary successors to the worklist.
  while (!worklist.is_empty()) {
    Node* node = worklist.Remove();
    node->MarkWith(!mark);
    node->UpdateRDIn(&worklist, mark);
  }

  // Step 4: Based on RD_in for block nodes, propagate reaching definitions
  // to all variable uses in the block.
  for (int i = 0; i < node_count; i++) {
    postorder_->at(i)->PropagateReachingDefinitions(&variables_);
  }
}


} }  // namespace v8::internal
