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

#include "flow-graph.h"

namespace v8 {
namespace internal {

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


// This function peels off one iteration of a for-loop. The return value
// is either a block statement containing the peeled loop or NULL in case
// there is a stack overflow.
static Statement* PeelForLoop(ForStatement* stmt) {
  // Mark this for-statement as processed.
  stmt->set_peel_this_loop(false);

  // Create new block containing the init statement of the for-loop and
  // an if-statement containing the peeled iteration and the original
  // loop without the init-statement.
  Block* block = new Block(NULL, 2, false);
  if (stmt->init() != NULL) {
    Statement* init = stmt->init();
    // The init statement gets the statement position of the for-loop
    // to make debugging of peeled loops possible.
    init->set_statement_pos(stmt->statement_pos());
    block->AddStatement(init);
  }

  // Copy the condition.
  CopyAstVisitor copy_visitor;
  Expression* cond_copy = stmt->cond() != NULL
      ? copy_visitor.DeepCopyExpr(stmt->cond())
      : new Literal(Factory::true_value());
  if (copy_visitor.HasStackOverflow()) return NULL;

  // Construct a block with the peeled body and the rest of the for-loop.
  Statement* body_copy = copy_visitor.DeepCopyStmt(stmt->body());
  if (copy_visitor.HasStackOverflow()) return NULL;

  Statement* next_copy = stmt->next() != NULL
      ? copy_visitor.DeepCopyStmt(stmt->next())
      : new EmptyStatement();
  if (copy_visitor.HasStackOverflow()) return NULL;

  Block* peeled_body = new Block(NULL, 3, false);
  peeled_body->AddStatement(body_copy);
  peeled_body->AddStatement(next_copy);
  peeled_body->AddStatement(stmt);

  // Remove the duplicated init statement from the for-statement.
  stmt->set_init(NULL);

  // Create new test at the top and add it to the newly created block.
  IfStatement* test = new IfStatement(cond_copy,
                                      peeled_body,
                                      new EmptyStatement());
  block->AddStatement(test);
  return block;
}


void FlowGraphBuilder::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    stmts->at(i) = ProcessStatement(stmts->at(i));
  }
}


Statement* FlowGraphBuilder::ProcessStatement(Statement* stmt) {
  if (FLAG_loop_peeling &&
      stmt->AsForStatement() != NULL &&
      stmt->AsForStatement()->peel_this_loop()) {
    Statement* tmp_stmt = PeelForLoop(stmt->AsForStatement());
    if (tmp_stmt == NULL) {
      SetStackOverflow();
    } else {
      stmt = tmp_stmt;
    }
  }
  Visit(stmt);
  return stmt;
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
  stmt->set_then_statement(ProcessStatement(stmt->then_statement()));

  FlowGraph left = graph_;
  graph_ = FlowGraph::Empty();
  stmt->set_else_statement(ProcessStatement(stmt->else_statement()));

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
  if (stmt->init() != NULL) stmt->set_init(ProcessStatement(stmt->init()));

  JoinNode* join = new JoinNode();
  FlowGraph original = graph_;
  graph_ = FlowGraph::Empty();
  if (stmt->cond() != NULL) Visit(stmt->cond());

  BranchNode* branch = new BranchNode();
  FlowGraph condition = graph_;
  graph_ = FlowGraph::Empty();
  stmt->set_body(ProcessStatement(stmt->body()));

  if (stmt->next() != NULL) stmt->set_next(ProcessStatement(stmt->next()));

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


void FlowGraphBuilder::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
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
      // The first definition in the body is numbered n, where n is the
      // number of parameters and stack-allocated locals.
      expr->set_num(body_definitions_.length() + variable_count_);
      body_definitions_.Add(expr);
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
    // The first definition in the body is numbered n, where n is the number
    // of parameters and stack-allocated locals.
    expr->set_num(body_definitions_.length() + variable_count_);
    body_definitions_.Add(expr);
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


} }  // namespace v8::internal
