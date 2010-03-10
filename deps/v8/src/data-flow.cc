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

namespace v8 {
namespace internal {


void FlowGraph::AppendInstruction(AstNode* instruction) {
  ASSERT(instruction != NULL);
  if (is_empty() || !exit()->IsBlockNode()) {
    AppendNode(new BlockNode());
  }
  BlockNode::cast(exit())->AddInstruction(instruction);
}


void FlowGraph::AppendNode(Node* node) {
  ASSERT(node != NULL);
  if (is_empty()) {
    entry_ = exit_ = node;
  } else {
    exit()->AddSuccessor(node);
    node->AddPredecessor(exit());
    exit_ = node;
  }
}


void FlowGraph::AppendGraph(FlowGraph* graph) {
  ASSERT(!graph->is_empty());
  if (is_empty()) {
    entry_ = graph->entry();
    exit_ = graph->exit();
  } else {
    exit()->AddSuccessor(graph->entry());
    graph->entry()->AddPredecessor(exit());
    exit_ = graph->exit();
  }
}


void FlowGraph::Split(BranchNode* branch,
                      FlowGraph* left,
                      FlowGraph* right,
                      JoinNode* merge) {
  // Graphs are in edge split form.  Add empty blocks if necessary.
  if (left->is_empty()) left->AppendNode(new BlockNode());
  if (right->is_empty()) right->AppendNode(new BlockNode());

  // Add the branch, left flowgraph and merge.
  AppendNode(branch);
  AppendGraph(left);
  AppendNode(merge);

  // Splice in the right flowgraph.
  right->AppendNode(merge);
  branch->AddSuccessor(right->entry());
  right->entry()->AddPredecessor(branch);
}


void FlowGraph::Loop(JoinNode* merge,
                     FlowGraph* condition,
                     BranchNode* branch,
                     FlowGraph* body) {
  // Add the merge, condition and branch.  Add merge's predecessors in
  // left-to-right order.
  AppendNode(merge);
  body->AppendNode(merge);
  AppendGraph(condition);
  AppendNode(branch);

  // Splice in the body flowgraph.
  branch->AddSuccessor(body->entry());
  body->entry()->AddPredecessor(branch);
}


void EntryNode::Traverse(bool mark,
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
  if (!successor0_->IsMarkedWith(mark)) {
    successor0_->MarkWith(mark);
    successor0_->Traverse(mark, preorder, postorder);
  }
  if (!successor1_->IsMarkedWith(mark)) {
    successor1_->MarkWith(mark);
    successor1_->Traverse(mark, preorder, postorder);
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
  graph_ = FlowGraph::Empty();
  graph_.AppendNode(new EntryNode());
  global_exit_ = new ExitNode();
  VisitStatements(lit->body());

  if (HasStackOverflow()) {
    graph_ = FlowGraph::Empty();
    return;
  }

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
  Visit(stmt->expression());
  graph_.AppendInstruction(stmt);
  graph_.AppendNode(global_exit());
}


void FlowGraphBuilder::VisitWithEnterStatement(WithEnterStatement* stmt) {
  Visit(stmt->expression());
  graph_.AppendInstruction(stmt);
}


void FlowGraphBuilder::VisitWithExitStatement(WithExitStatement* stmt) {
  graph_.AppendInstruction(stmt);
}


void FlowGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  JoinNode* join = new JoinNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->body());

  FlowGraph body = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->cond());

  BranchNode* branch = new BranchNode();

  // Add body, condition and branch.
  original.AppendNode(join);
  original.AppendGraph(&body);
  original.AppendGraph(&graph_);  // The condition.
  original.AppendNode(branch);

  // Tie the knot.
  branch->AddSuccessor(join);
  join->AddPredecessor(branch);

  graph_ = original;
}


void FlowGraphBuilder::VisitWhileStatement(WhileStatement* stmt) {
  JoinNode* join = new JoinNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->cond());

  BranchNode* branch = new BranchNode();
  FlowGraph condition = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->body());

  original.Loop(join, &condition, branch, &graph_);
  graph_ = original;
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

  original.Loop(join, &condition, branch, &graph_);
  graph_ = original;
}


void FlowGraphBuilder::VisitForInStatement(ForInStatement* stmt) {
  Visit(stmt->enumerable());

  JoinNode* join = new JoinNode();
  FlowGraph empty;
  BranchNode* branch = new BranchNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  Visit(stmt->body());

  original.Loop(join, &empty, branch, &graph_);
  graph_ = original;
}


void FlowGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  graph_.AppendInstruction(stmt);
}


void FlowGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitConditional(Conditional* expr) {
  Visit(expr->condition());

  BranchNode* branch = new BranchNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  Visit(expr->then_expression());

  FlowGraph left = graph_;
  graph_ = FlowGraph::Empty();
  Visit(expr->else_expression());

  JoinNode* join = new JoinNode();
  original.Split(branch, &left, &graph_, join);
  graph_ = original;
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
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();
  for (int i = 0, len = properties->length(); i < len; i++) {
    Visit(properties->at(i)->value());
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* values = expr->values();
  for (int i = 0, len = values->length(); i < len; i++) {
    Visit(values->at(i));
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitAssignment(Assignment* expr) {
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  // Left-hand side can be a variable or property (or reference error) but
  // not both.
  ASSERT(var == NULL || prop == NULL);
  if (var != NULL) {
    Visit(expr->value());
    if (var->IsStackAllocated()) definitions_.Add(expr);

  } else if (prop != NULL) {
    Visit(prop->obj());
    if (!prop->key()->IsPropertyName()) Visit(prop->key());
    Visit(expr->value());
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitThrow(Throw* expr) {
  Visit(expr->exception());
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitProperty(Property* expr) {
  Visit(expr->obj());
  if (!expr->key()->IsPropertyName()) Visit(expr->key());
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCall(Call* expr) {
  Visit(expr->expression());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    Visit(arguments->at(i));
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCallNew(CallNew* expr) {
  Visit(expr->expression());
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    Visit(arguments->at(i));
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* arguments = expr->arguments();
  for (int i = 0, len = arguments->length(); i < len; i++) {
    Visit(arguments->at(i));
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  Visit(expr->expression());
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitCountOperation(CountOperation* expr) {
  Visit(expr->expression());
  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  if (var != NULL && var->IsStackAllocated()) {
    definitions_.Add(expr);
  }
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  Visit(expr->left());

  switch (expr->op()) {
    case Token::COMMA:
      Visit(expr->right());
      break;

    case Token::OR: {
      BranchNode* branch = new BranchNode();
      FlowGraph original = graph_;
      graph_ = FlowGraph::Empty();
      Visit(expr->right());
      FlowGraph empty;
      JoinNode* join = new JoinNode();
      original.Split(branch, &empty, &graph_, join);
      graph_ = original;
      break;
    }

    case Token::AND: {
      BranchNode* branch = new BranchNode();
      FlowGraph original = graph_;
      graph_ = FlowGraph::Empty();
      Visit(expr->right());
      FlowGraph empty;
      JoinNode* join = new JoinNode();
      original.Split(branch, &graph_, &empty, join);
      graph_ = original;
      break;
    }

    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      Visit(expr->right());
      graph_.AppendInstruction(expr);
      break;

    default:
      UNREACHABLE();
  }
}


void FlowGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
  graph_.AppendInstruction(expr);
}


void FlowGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  graph_.AppendInstruction(expr);
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


#ifdef DEBUG

// Print a textual representation of an instruction in a flow graph.  Using
// the AstVisitor is overkill because there is no recursion here.  It is
// only used for printing in debug mode.
class TextInstructionPrinter: public AstVisitor {
 public:
  TextInstructionPrinter() {}

 private:
  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

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
    SmartPointer<char> name = var->name()->ToCString();
    PrintF("%s", *name);
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

  if (var != NULL) {
    SmartPointer<char> name = var->name()->ToCString();
    PrintF("%s %s @%d",
           *name,
           Token::String(expr->op()),
           expr->value()->num());
  } else if (prop != NULL) {
    if (prop->key()->IsPropertyName()) {
      PrintF("@%d.", prop->obj()->num());
      ASSERT(prop->key()->AsLiteral() != NULL);
      prop->key()->AsLiteral()->handle()->Print();
      PrintF(" %s @%d",
             Token::String(expr->op()),
             expr->value()->num());
    } else {
      PrintF("@%d[@%d] %s @%d",
             prop->obj()->num(),
             prop->key()->num(),
             Token::String(expr->op()),
             expr->value()->num());
    }
  } else {
    // Throw reference error.
    Visit(expr->target());
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
  SmartPointer<char> name = expr->name()->ToCString();
  PrintF("%s(", *name);
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


void Node::AssignNumbers() {
  set_number(node_count++);
}


void BlockNode::AssignNumbers() {
  set_number(node_count++);
  for (int i = 0, len = instructions_.length(); i < len; i++) {
    instructions_[i]->set_num(instruction_count++);
  }
}


void EntryNode::PrintText() {
  PrintF("L%d: Entry\n", number());
  PrintF("goto L%d\n\n", successor_->number());
}

void ExitNode::PrintText() {
  PrintF("L%d: Exit\n\n", number());
}


void BlockNode::PrintText() {
  // Print the instructions in the block.
  PrintF("L%d: Block\n", number());
  TextInstructionPrinter printer;
  for (int i = 0, len = instructions_.length(); i < len; i++) {
    PrintF("%d ", instructions_[i]->num());
    printer.Visit(instructions_[i]);
    PrintF("\n");
  }
  PrintF("goto L%d\n\n", successor_->number());
}


void BranchNode::PrintText() {
  PrintF("L%d: Branch\n", number());
  PrintF("goto (L%d, L%d)\n\n", successor0_->number(), successor1_->number());
}


void JoinNode::PrintText() {
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
    postorder->at(i)->AssignNumbers();
  }

  // Print basic blocks in reverse postorder.
  for (int i = postorder->length() - 1; i >= 0; i--) {
    postorder->at(i)->PrintText();
  }
}


#endif  // defined(DEBUG)


} }  // namespace v8::internal
