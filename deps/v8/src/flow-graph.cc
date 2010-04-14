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
#include "scopes.h"

namespace v8 {
namespace internal {

void BasicBlock::BuildTraversalOrder(ZoneList<BasicBlock*>* preorder,
                                     ZoneList<BasicBlock*>* postorder,
                                     bool mark) {
  if (mark_ == mark) return;
  mark_ = mark;
  preorder->Add(this);
  if (right_successor_ != NULL) {
    right_successor_->BuildTraversalOrder(preorder, postorder, mark);
  }
  if (left_successor_ != NULL) {
    left_successor_->BuildTraversalOrder(preorder, postorder, mark);
  }
  postorder->Add(this);
}


FlowGraph* FlowGraphBuilder::Build(FunctionLiteral* lit) {
  // Create new entry and exit nodes.  These will not change during
  // construction.
  entry_ = new BasicBlock(NULL);
  exit_ = new BasicBlock(NULL);
  // Begin accumulating instructions in the entry block.
  current_ = entry_;

  VisitDeclarations(lit->scope()->declarations());
  VisitStatements(lit->body());
  // In the event of stack overflow or failure to handle a syntactic
  // construct, return an invalid flow graph.
  if (HasStackOverflow()) return new FlowGraph(NULL, NULL);

  // If current is not the exit, add a link to the exit.
  if (current_ != exit_) {
    // If current already has a successor (i.e., will be a branch node) and
    // if the exit already has a predecessor, insert an empty block to
    // maintain edge split form.
    if (current_->HasSuccessor() && exit_->HasPredecessor()) {
      current_ = new BasicBlock(current_);
    }
    Literal* undefined = new Literal(Factory::undefined_value());
    current_->AddInstruction(new ReturnStatement(undefined));
    exit_->AddPredecessor(current_);
  }

  FlowGraph* graph = new FlowGraph(entry_, exit_);
  bool mark = !entry_->GetMark();
  entry_->BuildTraversalOrder(graph->preorder(), graph->postorder(), mark);

#ifdef DEBUG
  // Number the nodes in reverse postorder.
  int n = 0;
  for (int i = graph->postorder()->length() - 1; i >= 0; --i) {
    graph->postorder()->at(i)->set_number(n++);
  }
#endif

  return graph;
}


void FlowGraphBuilder::VisitDeclaration(Declaration* decl) {
  Variable* var = decl->proxy()->AsVariable();
  Slot* slot = var->slot();
  // We allow only declarations that do not require code generation.
  // The following all require code generation: global variables and
  // functions, variables with slot type LOOKUP, declarations with
  // mode CONST, and functions.

  if (var->is_global() ||
      (slot != NULL && slot->type() == Slot::LOOKUP) ||
      decl->mode() == Variable::CONST ||
      decl->fun() != NULL) {
    // Here and in the rest of the flow graph builder we indicate an
    // unsupported syntactic construct by setting the stack overflow
    // flag on the visitor.  This causes bailout of the visitor.
    SetStackOverflow();
  }
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
  // Build a diamond in the flow graph.  First accumulate the instructions
  // of the test in the current basic block.
  Visit(stmt->condition());

  // Remember the branch node and accumulate the true branch as its left
  // successor.  This relies on the successors being added left to right.
  BasicBlock* branch = current_;
  current_ = new BasicBlock(branch);
  Visit(stmt->then_statement());

  // Construct a join node and then accumulate the false branch in a fresh
  // successor of the branch node.
  BasicBlock* join = new BasicBlock(current_);
  current_ = new BasicBlock(branch);
  Visit(stmt->else_statement());
  join->AddPredecessor(current_);

  current_ = join;
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
  // Build a loop in the flow graph.  First accumulate the instructions of
  // the initializer in the current basic block.
  if (stmt->init() != NULL) Visit(stmt->init());

  // Create a new basic block for the test.  This will be the join node.
  BasicBlock* join = new BasicBlock(current_);
  current_ = join;
  if (stmt->cond() != NULL) Visit(stmt->cond());

  // The current node is the branch node.  Create a new basic block to begin
  // the body.
  BasicBlock* branch = current_;
  current_ = new BasicBlock(branch);
  Visit(stmt->body());
  if (stmt->next() != NULL) Visit(stmt->next());

  // Add the backward edge from the end of the body and continue with the
  // false arm of the branch.
  join->AddPredecessor(current_);
  current_ = new BasicBlock(branch);
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
  // Slots do not appear in the AST.
  UNREACHABLE();
}


void FlowGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  current_->AddInstruction(expr);
}


void FlowGraphBuilder::VisitLiteral(Literal* expr) {
  current_->AddInstruction(expr);
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
  // There are three basic kinds of assignment: variable assignments,
  // property assignments, and invalid left-hand sides (which are translated
  // to "throw ReferenceError" by the parser).
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  ASSERT(var == NULL || prop == NULL);
  if (var != NULL) {
    if (expr->is_compound() && !expr->target()->IsTrivial()) {
      Visit(expr->target());
    }
    if (!expr->value()->IsTrivial()) Visit(expr->value());
    current_->AddInstruction(expr);

  } else if (prop != NULL) {
    if (!prop->obj()->IsTrivial()) Visit(prop->obj());
    if (!prop->key()->IsPropertyName() && !prop->key()->IsTrivial()) {
      Visit(prop->key());
    }
    if (!expr->value()->IsTrivial()) Visit(expr->value());
    current_->AddInstruction(expr);

  } else {
    Visit(expr->target());
  }
}


void FlowGraphBuilder::VisitThrow(Throw* expr) {
  SetStackOverflow();
}


void FlowGraphBuilder::VisitProperty(Property* expr) {
  if (!expr->obj()->IsTrivial()) Visit(expr->obj());
  if (!expr->key()->IsPropertyName() && !expr->key()->IsTrivial()) {
    Visit(expr->key());
  }
  current_->AddInstruction(expr);
}


void FlowGraphBuilder::VisitCall(Call* expr) {
  Visit(expr->expression());
  VisitExpressions(expr->arguments());
  current_->AddInstruction(expr);
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
      current_->AddInstruction(expr);
      break;

    default:
      UNREACHABLE();
  }
}


void FlowGraphBuilder::VisitCountOperation(CountOperation* expr) {
  Visit(expr->expression());
  current_->AddInstruction(expr);
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
    case Token::SAR:
    case Token::SHR:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      if (!expr->left()->IsTrivial()) Visit(expr->left());
      if (!expr->right()->IsTrivial()) Visit(expr->right());
      current_->AddInstruction(expr);
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
      if (!expr->left()->IsTrivial()) Visit(expr->left());
      if (!expr->right()->IsTrivial()) Visit(expr->right());
      current_->AddInstruction(expr);
      break;

    default:
      UNREACHABLE();
  }
}


void FlowGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  SetStackOverflow();
}


#ifdef DEBUG

// Print a textual representation of an instruction in a flow graph.
class InstructionPrinter: public AstVisitor {
 public:
  InstructionPrinter() {}

 private:
  // Overridden from the base class.
  virtual void VisitExpressions(ZoneList<Expression*>* exprs);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(InstructionPrinter);
};


static void PrintSubexpression(Expression* expr) {
  if (!expr->IsTrivial()) {
    PrintF("@%d", expr->num());
  } else if (expr->AsLiteral() != NULL) {
    expr->AsLiteral()->handle()->Print();
  } else if (expr->AsVariableProxy() != NULL) {
    PrintF("%s", *expr->AsVariableProxy()->name()->ToCString());
  } else {
    UNREACHABLE();
  }
}


void InstructionPrinter::VisitExpressions(ZoneList<Expression*>* exprs) {
  for (int i = 0; i < exprs->length(); ++i) {
    if (i != 0) PrintF(", ");
    PrintF("@%d", exprs->at(i)->num());
  }
}


// We only define printing functions for the node types that can occur as
// instructions in a flow graph.  The rest are unreachable.
void InstructionPrinter::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void InstructionPrinter::VisitBlock(Block* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitExpressionStatement(ExpressionStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitEmptyStatement(EmptyStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitReturnStatement(ReturnStatement* stmt) {
  PrintF("return ");
  PrintSubexpression(stmt->expression());
}


void InstructionPrinter::VisitWithEnterStatement(WithEnterStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitDebuggerStatement(DebuggerStatement* stmt) {
  UNREACHABLE();
}


void InstructionPrinter::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->AsVariable();
  if (var != NULL) {
    PrintF("%s", *var->name()->ToCString());
  } else {
    ASSERT(expr->AsProperty() != NULL);
    Visit(expr->AsProperty());
  }
}


void InstructionPrinter::VisitLiteral(Literal* expr) {
  expr->handle()->Print();
}


void InstructionPrinter::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitAssignment(Assignment* expr) {
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();

  // Print the left-hand side.
  Visit(expr->target());
  if (var == NULL && prop == NULL) return;  // Throw reference error.
  PrintF(" = ");
  // For compound assignments, print the left-hand side again and the
  // corresponding binary operator.
  if (expr->is_compound()) {
    PrintSubexpression(expr->target());
    PrintF(" %s ", Token::String(expr->binary_op()));
  }

  // Print the right-hand side.
  PrintSubexpression(expr->value());
}


void InstructionPrinter::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitProperty(Property* expr) {
  PrintSubexpression(expr->obj());
  if (expr->key()->IsPropertyName()) {
    PrintF(".");
    ASSERT(expr->key()->AsLiteral() != NULL);
    expr->key()->AsLiteral()->handle()->Print();
  } else {
    PrintF("[");
    PrintSubexpression(expr->key());
    PrintF("]");
  }
}


void InstructionPrinter::VisitCall(Call* expr) {
  PrintF("@%d(", expr->expression()->num());
  VisitExpressions(expr->arguments());
  PrintF(")");
}


void InstructionPrinter::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void InstructionPrinter::VisitUnaryOperation(UnaryOperation* expr) {
  PrintF("%s(@%d)", Token::String(expr->op()), expr->expression()->num());
}


void InstructionPrinter::VisitCountOperation(CountOperation* expr) {
  if (expr->is_prefix()) {
    PrintF("%s@%d", Token::String(expr->op()), expr->expression()->num());
  } else {
    PrintF("@%d%s", expr->expression()->num(), Token::String(expr->op()));
  }
}


void InstructionPrinter::VisitBinaryOperation(BinaryOperation* expr) {
  PrintSubexpression(expr->left());
  PrintF(" %s ", Token::String(expr->op()));
  PrintSubexpression(expr->right());
}


void InstructionPrinter::VisitCompareOperation(CompareOperation* expr) {
  PrintSubexpression(expr->left());
  PrintF(" %s ", Token::String(expr->op()));
  PrintSubexpression(expr->right());
}


void InstructionPrinter::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


int BasicBlock::PrintAsText(int instruction_number) {
  // Print a label for all blocks except the entry.
  if (HasPredecessor()) {
    PrintF("L%d:", number());
  }

  // Number and print the instructions.  Since AST child nodes are visited
  // before their parents, the parent nodes can refer to them by number.
  InstructionPrinter printer;
  for (int i = 0; i < instructions_.length(); ++i) {
    PrintF("\n%d ", instruction_number);
    instructions_[i]->set_num(instruction_number++);
    instructions_[i]->Accept(&printer);
  }

  // If this is the exit, print "exit".  If there is a single successor,
  // print "goto" successor on a separate line.  If there are two
  // successors, print "goto" successor on the same line as the last
  // instruction in the block.  There is a blank line between blocks (and
  // after the last one).
  if (left_successor_ == NULL) {
    PrintF("\nexit\n\n");
  } else if (right_successor_ == NULL) {
    PrintF("\ngoto L%d\n\n", left_successor_->number());
  } else {
    PrintF(", goto (L%d, L%d)\n\n",
           left_successor_->number(),
           right_successor_->number());
  }

  return instruction_number;
}


void FlowGraph::PrintAsText(Handle<String> name) {
  PrintF("\n==== name = \"%s\" ====\n", *name->ToCString());
  // Print nodes in reverse postorder.  Note that AST node numbers are used
  // during printing of instructions and thus their current values are
  // destroyed.
  int number = 0;
  for (int i = postorder_.length() - 1; i >= 0; --i) {
    number = postorder_[i]->PrintAsText(number);
  }
}

#endif  // DEBUG


} }  // namespace v8::internal
