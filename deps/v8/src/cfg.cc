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

#include "bootstrapper.h"
#include "cfg.h"
#include "scopeinfo.h"
#include "scopes.h"

namespace v8 {
namespace internal {


CfgGlobals* CfgGlobals::top_ = NULL;


CfgGlobals::CfgGlobals(FunctionLiteral* fun)
    : global_fun_(fun),
      global_exit_(new ExitNode()),
#ifdef DEBUG
      node_counter_(0),
#endif
      previous_(top_) {
  top_ = this;
}


#define BAILOUT(reason)                         \
  do { return NULL; } while (false)

Cfg* Cfg::Build() {
  FunctionLiteral* fun = CfgGlobals::current()->fun();
  if (fun->scope()->num_heap_slots() > 0) {
    BAILOUT("function has context slots");
  }
  if (fun->scope()->arguments() != NULL) {
    BAILOUT("function uses .arguments");
  }

  ZoneList<Statement*>* body = fun->body();
  if (body->is_empty()) {
    BAILOUT("empty function body");
  }

  StatementBuilder builder;
  builder.VisitStatements(body);
  Cfg* cfg = builder.cfg();
  if (cfg == NULL) {
    BAILOUT("unsupported statement type");
  }
  if (cfg->has_exit()) {
    BAILOUT("control path without explicit return");
  }
  cfg->PrependEntryNode();
  return cfg;
}

#undef BAILOUT


void Cfg::PrependEntryNode() {
  ASSERT(!is_empty());
  entry_ = new EntryNode(InstructionBlock::cast(entry()));
}


void Cfg::Append(Instruction* instr) {
  ASSERT(has_exit());
  ASSERT(!is_empty());
  InstructionBlock::cast(exit_)->Append(instr);
}


void Cfg::AppendReturnInstruction(Value* value) {
  Append(new ReturnInstr(value));
  ExitNode* global_exit = CfgGlobals::current()->exit();
  InstructionBlock::cast(exit_)->set_successor(global_exit);
  exit_ = NULL;
}


void InstructionBlock::Unmark() {
  if (is_marked_) {
    is_marked_ = false;
    successor_->Unmark();
  }
}


void EntryNode::Unmark() {
  if (is_marked_) {
    is_marked_ = false;
    successor_->Unmark();
  }
}


void ExitNode::Unmark() {
  is_marked_ = false;
}


Handle<Code> Cfg::Compile(Handle<Script> script) {
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler* masm = new MacroAssembler(NULL, kInitialBufferSize);
  entry()->Compile(masm);
  entry()->Unmark();
  CodeDesc desc;
  masm->GetCode(&desc);
  FunctionLiteral* fun = CfgGlobals::current()->fun();
  ZoneScopeInfo info(fun->scope());
  InLoopFlag in_loop = fun->loop_nesting() ? IN_LOOP : NOT_IN_LOOP;
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, in_loop);
  Handle<Code> code = Factory::NewCode(desc, &info, flags, masm->CodeObject());

  // Add unresolved entries in the code to the fixup list.
  Bootstrapper::AddFixup(*code, masm);

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code) {
    // Print the source code if available.
    if (!script->IsUndefined() && !script->source()->IsUndefined()) {
      PrintF("--- Raw source ---\n");
      StringInputBuffer stream(String::cast(script->source()));
      stream.Seek(fun->start_position());
      // fun->end_position() points to the last character in the
      // stream. We need to compensate by adding one to calculate the
      // length.
      int source_len = fun->end_position() - fun->start_position() + 1;
      for (int i = 0; i < source_len; i++) {
        if (stream.has_more()) PrintF("%c", stream.GetNext());
      }
      PrintF("\n\n");
    }
    PrintF("--- Code ---\n");
    code->Disassemble(*fun->name()->ToCString());
  }
#endif

  return code;
}


// The expression builder should not be used for declarations or statements.
void ExpressionBuilder::VisitDeclaration(Declaration* decl) { UNREACHABLE(); }

#define DEFINE_VISIT(type)                                              \
  void ExpressionBuilder::Visit##type(type* stmt) { UNREACHABLE(); }
STATEMENT_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT


// Macros (temporarily) handling unsupported expression types.
#define BAILOUT(reason)                         \
  do {                                          \
    value_ = NULL;                              \
    return;                                     \
  } while (false)

#define CHECK_BAILOUT()                         \
  if (value_ == NULL) { return; } else {}

void ExpressionBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  BAILOUT("FunctionLiteral");
}


void ExpressionBuilder::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  BAILOUT("FunctionBoilerplateLiteral");
}


void ExpressionBuilder::VisitConditional(Conditional* expr) {
  BAILOUT("Conditional");
}


void ExpressionBuilder::VisitSlot(Slot* expr) {
  BAILOUT("Slot");
}


void ExpressionBuilder::VisitVariableProxy(VariableProxy* expr) {
  Expression* rewrite = expr->var()->rewrite();
  if (rewrite == NULL || rewrite->AsSlot() == NULL) {
    BAILOUT("unsupported variable (not a slot)");
  }
  Slot* slot = rewrite->AsSlot();
  if (slot->type() != Slot::PARAMETER && slot->type() != Slot::LOCAL) {
    BAILOUT("unsupported slot type (not a parameter or local)");
  }
  value_ = new SlotLocation(slot->type(), slot->index());
}


void ExpressionBuilder::VisitLiteral(Literal* expr) {
  value_ = new Constant(expr->handle());
}


void ExpressionBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  BAILOUT("RegExpLiteral");
}


void ExpressionBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  BAILOUT("ObjectLiteral");
}


void ExpressionBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  BAILOUT("ArrayLiteral");
}


void ExpressionBuilder::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  BAILOUT("CatchExtensionObject");
}


void ExpressionBuilder::VisitAssignment(Assignment* expr) {
  BAILOUT("Assignment");
}


void ExpressionBuilder::VisitThrow(Throw* expr) {
  BAILOUT("Throw");
}


void ExpressionBuilder::VisitProperty(Property* expr) {
  BAILOUT("Property");
}


void ExpressionBuilder::VisitCall(Call* expr) {
  BAILOUT("Call");
}


void ExpressionBuilder::VisitCallEval(CallEval* expr) {
  BAILOUT("CallEval");
}


void ExpressionBuilder::VisitCallNew(CallNew* expr) {
  BAILOUT("CallNew");
}


void ExpressionBuilder::VisitCallRuntime(CallRuntime* expr) {
  BAILOUT("CallRuntime");
}


void ExpressionBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  BAILOUT("UnaryOperation");
}


void ExpressionBuilder::VisitCountOperation(CountOperation* expr) {
  BAILOUT("CountOperation");
}


void ExpressionBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  BAILOUT("BinaryOperation");
}


void ExpressionBuilder::VisitCompareOperation(CompareOperation* expr) {
  BAILOUT("CompareOperation");
}


void ExpressionBuilder::VisitThisFunction(ThisFunction* expr) {
  BAILOUT("ThisFunction");
}

#undef BAILOUT
#undef CHECK_BAILOUT


// Macros (temporarily) handling unsupported statement types.
#define BAILOUT(reason)                         \
  do {                                          \
    cfg_ = NULL;                                \
    return;                                     \
  } while (false)

#define CHECK_BAILOUT()                         \
  if (cfg_ == NULL) { return; } else {}

void StatementBuilder::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
    CHECK_BAILOUT();
    if (!cfg_->has_exit()) return;
  }
}


// The statement builder should not be used for declarations or expressions.
void StatementBuilder::VisitDeclaration(Declaration* decl) { UNREACHABLE(); }

#define DEFINE_VISIT(type)                                      \
  void StatementBuilder::Visit##type(type* expr) { UNREACHABLE(); }
EXPRESSION_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT


void StatementBuilder::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void StatementBuilder::VisitExpressionStatement(ExpressionStatement* stmt) {
  BAILOUT("ExpressionStatement");
}


void StatementBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  // Nothing to do.
}


void StatementBuilder::VisitIfStatement(IfStatement* stmt) {
  BAILOUT("IfStatement");
}


void StatementBuilder::VisitContinueStatement(ContinueStatement* stmt) {
  BAILOUT("ContinueStatement");
}


void StatementBuilder::VisitBreakStatement(BreakStatement* stmt) {
  BAILOUT("BreakStatement");
}


void StatementBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  ExpressionBuilder builder;
  builder.Visit(stmt->expression());
  Value* value = builder.value();
  if (value == NULL) BAILOUT("unsupported expression type");
  cfg_->AppendReturnInstruction(value);
}


void StatementBuilder::VisitWithEnterStatement(WithEnterStatement* stmt) {
  BAILOUT("WithEnterStatement");
}


void StatementBuilder::VisitWithExitStatement(WithExitStatement* stmt) {
  BAILOUT("WithExitStatement");
}


void StatementBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  BAILOUT("SwitchStatement");
}


void StatementBuilder::VisitLoopStatement(LoopStatement* stmt) {
  BAILOUT("LoopStatement");
}


void StatementBuilder::VisitForInStatement(ForInStatement* stmt) {
  BAILOUT("ForInStatement");
}


void StatementBuilder::VisitTryCatch(TryCatch* stmt) {
  BAILOUT("TryCatch");
}


void StatementBuilder::VisitTryFinally(TryFinally* stmt) {
  BAILOUT("TryFinally");
}


void StatementBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  BAILOUT("DebuggerStatement");
}


#ifdef DEBUG
// CFG printing support (via depth-first, preorder block traversal).

void Cfg::Print() {
  entry_->Print();
  entry_->Unmark();
}


void Constant::Print() {
  PrintF("Constant(");
  handle_->Print();
  PrintF(")");
}


void SlotLocation::Print() {
  PrintF("Slot(");
  switch (type_) {
    case Slot::PARAMETER:
      PrintF("PARAMETER, %d)", index_);
      break;
    case Slot::LOCAL:
      PrintF("LOCAL, %d)", index_);
      break;
    default:
      UNREACHABLE();
  }
}


void ReturnInstr::Print() {
  PrintF("Return ");
  value_->Print();
  PrintF("\n");
}


void InstructionBlock::Print() {
  if (!is_marked_) {
    is_marked_ = true;
    PrintF("L%d:\n", number());
    for (int i = 0, len = instructions_.length(); i < len; i++) {
      instructions_[i]->Print();
    }
    PrintF("Goto L%d\n\n", successor_->number());
    successor_->Print();
  }
}


void EntryNode::Print() {
  if (!is_marked_) {
    is_marked_ = true;
    successor_->Print();
  }
}


void ExitNode::Print() {
  if (!is_marked_) {
    is_marked_ = true;
    PrintF("L%d:\nExit\n\n", number());
  }
}

#endif  // DEBUG

} }  // namespace v8::internal
