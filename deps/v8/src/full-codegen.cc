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
#include "full-codegen.h"
#include "scopes.h"
#include "stub-cache.h"
#include "debug.h"
#include "liveedit.h"

namespace v8 {
namespace internal {

#define BAILOUT(reason)                         \
  do {                                          \
    if (FLAG_trace_bailout) {                   \
      PrintF("%s\n", reason);                   \
    }                                           \
    has_supported_syntax_ = false;              \
    return;                                     \
  } while (false)


#define CHECK_BAILOUT                           \
  do {                                          \
    if (!has_supported_syntax_) return;         \
  } while (false)


void FullCodeGenSyntaxChecker::Check(FunctionLiteral* fun) {
  Scope* scope = fun->scope();
  VisitDeclarations(scope->declarations());
  CHECK_BAILOUT;

  VisitStatements(fun->body());
}


void FullCodeGenSyntaxChecker::VisitDeclarations(
    ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); i++) {
    Visit(decls->at(i));
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitDeclaration(Declaration* decl) {
  Property* prop = decl->proxy()->AsProperty();
  if (prop != NULL) {
    Visit(prop->obj());
    Visit(prop->key());
  }

  if (decl->fun() != NULL) {
    Visit(decl->fun());
  }
}


void FullCodeGenSyntaxChecker::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void FullCodeGenSyntaxChecker::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void FullCodeGenSyntaxChecker::VisitEmptyStatement(EmptyStatement* stmt) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitIfStatement(IfStatement* stmt) {
  Visit(stmt->condition());
  CHECK_BAILOUT;
  Visit(stmt->then_statement());
  CHECK_BAILOUT;
  Visit(stmt->else_statement());
}


void FullCodeGenSyntaxChecker::VisitContinueStatement(ContinueStatement* stmt) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitBreakStatement(BreakStatement* stmt) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitReturnStatement(ReturnStatement* stmt) {
  Visit(stmt->expression());
}


void FullCodeGenSyntaxChecker::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  Visit(stmt->expression());
}


void FullCodeGenSyntaxChecker::VisitWithExitStatement(WithExitStatement* stmt) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitSwitchStatement(SwitchStatement* stmt) {
  BAILOUT("SwitchStatement");
}


void FullCodeGenSyntaxChecker::VisitDoWhileStatement(DoWhileStatement* stmt) {
  Visit(stmt->cond());
  CHECK_BAILOUT;
  Visit(stmt->body());
}


void FullCodeGenSyntaxChecker::VisitWhileStatement(WhileStatement* stmt) {
  Visit(stmt->cond());
  CHECK_BAILOUT;
  Visit(stmt->body());
}


void FullCodeGenSyntaxChecker::VisitForStatement(ForStatement* stmt) {
  if (!FLAG_always_full_compiler) BAILOUT("ForStatement");
  if (stmt->init() != NULL) {
    Visit(stmt->init());
    CHECK_BAILOUT;
  }
  if (stmt->cond() != NULL) {
    Visit(stmt->cond());
    CHECK_BAILOUT;
  }
  Visit(stmt->body());
  if (stmt->next() != NULL) {
    CHECK_BAILOUT;
    Visit(stmt->next());
  }
}


void FullCodeGenSyntaxChecker::VisitForInStatement(ForInStatement* stmt) {
  BAILOUT("ForInStatement");
}


void FullCodeGenSyntaxChecker::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Visit(stmt->try_block());
  CHECK_BAILOUT;
  Visit(stmt->catch_block());
}


void FullCodeGenSyntaxChecker::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  Visit(stmt->try_block());
  CHECK_BAILOUT;
  Visit(stmt->finally_block());
}


void FullCodeGenSyntaxChecker::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  BAILOUT("SharedFunctionInfoLiteral");
}


void FullCodeGenSyntaxChecker::VisitConditional(Conditional* expr) {
  Visit(expr->condition());
  CHECK_BAILOUT;
  Visit(expr->then_expression());
  CHECK_BAILOUT;
  Visit(expr->else_expression());
}


void FullCodeGenSyntaxChecker::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void FullCodeGenSyntaxChecker::VisitVariableProxy(VariableProxy* expr) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitLiteral(Literal* expr) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitRegExpLiteral(RegExpLiteral* expr) {
  // Supported.
}


void FullCodeGenSyntaxChecker::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();

  for (int i = 0, len = properties->length(); i < len; i++) {
    ObjectLiteral::Property* property = properties->at(i);
    if (property->IsCompileTimeValue()) continue;
    Visit(property->key());
    CHECK_BAILOUT;
    Visit(property->value());
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* subexprs = expr->values();
  for (int i = 0, len = subexprs->length(); i < len; i++) {
    Expression* subexpr = subexprs->at(i);
    if (subexpr->AsLiteral() != NULL) continue;
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;
    Visit(subexpr);
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  Visit(expr->key());
  CHECK_BAILOUT;
  Visit(expr->value());
}


void FullCodeGenSyntaxChecker::VisitAssignment(Assignment* expr) {
  Token::Value op = expr->op();
  if (op == Token::INIT_CONST) BAILOUT("initialize constant");

  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  ASSERT(var == NULL || prop == NULL);
  if (var != NULL) {
    if (var->mode() == Variable::CONST) BAILOUT("Assignment to const");
    // All other variables are supported.
  } else if (prop != NULL) {
    Visit(prop->obj());
    CHECK_BAILOUT;
    Visit(prop->key());
    CHECK_BAILOUT;
  } else {
    // This is a throw reference error.
    BAILOUT("non-variable/non-property assignment");
  }

  Visit(expr->value());
}


void FullCodeGenSyntaxChecker::VisitThrow(Throw* expr) {
  Visit(expr->exception());
}


void FullCodeGenSyntaxChecker::VisitProperty(Property* expr) {
  Visit(expr->obj());
  CHECK_BAILOUT;
  Visit(expr->key());
}


void FullCodeGenSyntaxChecker::VisitCall(Call* expr) {
  Expression* fun = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  // Check for supported calls
  if (var != NULL && var->is_possibly_eval()) {
    BAILOUT("call to the identifier 'eval'");
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Calls to global variables are supported.
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    BAILOUT("call to a lookup slot");
  } else if (fun->AsProperty() != NULL) {
    Property* prop = fun->AsProperty();
    Visit(prop->obj());
    CHECK_BAILOUT;
    Visit(prop->key());
    CHECK_BAILOUT;
  } else {
    // Otherwise the call is supported if the function expression is.
    Visit(fun);
  }
  // Check all arguments to the call.
  for (int i = 0; i < args->length(); i++) {
    Visit(args->at(i));
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitCallNew(CallNew* expr) {
  Visit(expr->expression());
  CHECK_BAILOUT;
  ZoneList<Expression*>* args = expr->arguments();
  // Check all arguments to the call
  for (int i = 0; i < args->length(); i++) {
    Visit(args->at(i));
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitCallRuntime(CallRuntime* expr) {
  // Check for inline runtime call
  if (expr->name()->Get(0) == '_' &&
      CodeGenerator::FindInlineRuntimeLUT(expr->name()) != NULL) {
    BAILOUT("inlined runtime call");
  }
  // Check all arguments to the call.  (Relies on TEMP meaning STACK.)
  for (int i = 0; i < expr->arguments()->length(); i++) {
    Visit(expr->arguments()->at(i));
    CHECK_BAILOUT;
  }
}


void FullCodeGenSyntaxChecker::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::ADD:
    case Token::BIT_NOT:
    case Token::NOT:
    case Token::SUB:
    case Token::TYPEOF:
    case Token::VOID:
      Visit(expr->expression());
      break;
    case Token::DELETE:
      BAILOUT("UnaryOperation: DELETE");
    default:
      UNREACHABLE();
  }
}


void FullCodeGenSyntaxChecker::VisitCountOperation(CountOperation* expr) {
  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  Property* prop = expr->expression()->AsProperty();
  ASSERT(var == NULL || prop == NULL);
  if (var != NULL) {
    // All global variables are supported.
    if (!var->is_global()) {
      ASSERT(var->slot() != NULL);
      Slot::Type type = var->slot()->type();
      if (type == Slot::LOOKUP) {
        BAILOUT("CountOperation with lookup slot");
      }
    }
  } else if (prop != NULL) {
    Visit(prop->obj());
    CHECK_BAILOUT;
    Visit(prop->key());
    CHECK_BAILOUT;
  } else {
    // This is a throw reference error.
    BAILOUT("CountOperation non-variable/non-property expression");
  }
}


void FullCodeGenSyntaxChecker::VisitBinaryOperation(BinaryOperation* expr) {
  Visit(expr->left());
  CHECK_BAILOUT;
  Visit(expr->right());
}


void FullCodeGenSyntaxChecker::VisitCompareOperation(CompareOperation* expr) {
  Visit(expr->left());
  CHECK_BAILOUT;
  Visit(expr->right());
}


void FullCodeGenSyntaxChecker::VisitThisFunction(ThisFunction* expr) {
  // Supported.
}

#undef BAILOUT
#undef CHECK_BAILOUT


void BreakableStatementChecker::Check(Statement* stmt) {
  Visit(stmt);
}


void BreakableStatementChecker::Check(Expression* expr) {
  Visit(expr);
}


void BreakableStatementChecker::VisitDeclaration(Declaration* decl) {
}


void BreakableStatementChecker::VisitBlock(Block* stmt) {
}


void BreakableStatementChecker::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  // Check if expression is breakable.
  Visit(stmt->expression());
}


void BreakableStatementChecker::VisitEmptyStatement(EmptyStatement* stmt) {
}


void BreakableStatementChecker::VisitIfStatement(IfStatement* stmt) {
  // If the condition is breakable the if statement is breakable.
  Visit(stmt->condition());
}


void BreakableStatementChecker::VisitContinueStatement(
    ContinueStatement* stmt) {
}


void BreakableStatementChecker::VisitBreakStatement(BreakStatement* stmt) {
}


void BreakableStatementChecker::VisitReturnStatement(ReturnStatement* stmt) {
  // Return is breakable if the expression is.
  Visit(stmt->expression());
}


void BreakableStatementChecker::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  Visit(stmt->expression());
}


void BreakableStatementChecker::VisitWithExitStatement(
    WithExitStatement* stmt) {
}


void BreakableStatementChecker::VisitSwitchStatement(SwitchStatement* stmt) {
  // Switch statements breakable if the tag expression is.
  Visit(stmt->tag());
}


void BreakableStatementChecker::VisitDoWhileStatement(DoWhileStatement* stmt) {
  // Mark do while as breakable to avoid adding a break slot in front of it.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitWhileStatement(WhileStatement* stmt) {
  // Mark while statements breakable if the condition expression is.
  Visit(stmt->cond());
}


void BreakableStatementChecker::VisitForStatement(ForStatement* stmt) {
  // Mark for statements breakable if the condition expression is.
  if (stmt->cond() != NULL) {
    Visit(stmt->cond());
  }
}


void BreakableStatementChecker::VisitForInStatement(ForInStatement* stmt) {
  // Mark for in statements breakable if the enumerable expression is.
  Visit(stmt->enumerable());
}


void BreakableStatementChecker::VisitTryCatchStatement(
    TryCatchStatement* stmt) {
  // Mark try catch as breakable to avoid adding a break slot in front of it.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  // Mark try finally as breakable to avoid adding a break slot in front of it.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  // The debugger statement is breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitFunctionLiteral(FunctionLiteral* expr) {
}


void BreakableStatementChecker::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
}


void BreakableStatementChecker::VisitConditional(Conditional* expr) {
}


void BreakableStatementChecker::VisitSlot(Slot* expr) {
}


void BreakableStatementChecker::VisitVariableProxy(VariableProxy* expr) {
}


void BreakableStatementChecker::VisitLiteral(Literal* expr) {
}


void BreakableStatementChecker::VisitRegExpLiteral(RegExpLiteral* expr) {
}


void BreakableStatementChecker::VisitObjectLiteral(ObjectLiteral* expr) {
}


void BreakableStatementChecker::VisitArrayLiteral(ArrayLiteral* expr) {
}


void BreakableStatementChecker::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
}


void BreakableStatementChecker::VisitAssignment(Assignment* expr) {
  // If assigning to a property (including a global property) the assignment is
  // breakable.
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  if (prop != NULL || (var != NULL && var->is_global())) {
    is_breakable_ = true;
    return;
  }

  // Otherwise the assignment is breakable if the assigned value is.
  Visit(expr->value());
}


void BreakableStatementChecker::VisitThrow(Throw* expr) {
  // Throw is breakable if the expression is.
  Visit(expr->exception());
}


void BreakableStatementChecker::VisitProperty(Property* expr) {
  // Property load is breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitCall(Call* expr) {
  // Function calls both through IC and call stub are breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitCallNew(CallNew* expr) {
  // Function calls through new are breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitCallRuntime(CallRuntime* expr) {
}


void BreakableStatementChecker::VisitUnaryOperation(UnaryOperation* expr) {
  Visit(expr->expression());
}


void BreakableStatementChecker::VisitCountOperation(CountOperation* expr) {
  Visit(expr->expression());
}


void BreakableStatementChecker::VisitBinaryOperation(BinaryOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
}


void BreakableStatementChecker::VisitCompareOperation(CompareOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
}


void BreakableStatementChecker::VisitThisFunction(ThisFunction* expr) {
}


#define __ ACCESS_MASM(masm())

Handle<Code> FullCodeGenerator::MakeCode(CompilationInfo* info) {
  Handle<Script> script = info->script();
  if (!script->IsUndefined() && !script->source()->IsUndefined()) {
    int len = String::cast(script->source())->length();
    Counters::total_full_codegen_source_size.Increment(len);
  }
  CodeGenerator::MakeCodePrologue(info);
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(NULL, kInitialBufferSize);

  FullCodeGenerator cgen(&masm);
  cgen.Generate(info, PRIMARY);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, NOT_IN_LOOP);
  return CodeGenerator::MakeCodeEpilogue(&masm, flags, info);
}


int FullCodeGenerator::SlotOffset(Slot* slot) {
  ASSERT(slot != NULL);
  // Offset is negative because higher indexes are at lower addresses.
  int offset = -slot->index() * kPointerSize;
  // Adjust by a (parameter or local) base offset.
  switch (slot->type()) {
    case Slot::PARAMETER:
      offset += (scope()->num_parameters() + 1) * kPointerSize;
      break;
    case Slot::LOCAL:
      offset += JavaScriptFrameConstants::kLocal0Offset;
      break;
    case Slot::CONTEXT:
    case Slot::LOOKUP:
      UNREACHABLE();
  }
  return offset;
}


void FullCodeGenerator::VisitDeclarations(
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
          Handle<SharedFunctionInfo> function =
              Compiler::BuildFunctionInfo(decl->fun(), script(), this);
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


void FullCodeGenerator::SetFunctionPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, fun->start_position());
  }
}


void FullCodeGenerator::SetReturnPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, fun->end_position());
  }
}


void FullCodeGenerator::SetStatementPosition(Statement* stmt) {
  if (FLAG_debug_info) {
#ifdef ENABLE_DEBUGGER_SUPPORT
    if (!Debugger::IsDebuggerActive()) {
      CodeGenerator::RecordPositions(masm_, stmt->statement_pos());
    } else {
      // Check if the statement will be breakable without adding a debug break
      // slot.
      BreakableStatementChecker checker;
      checker.Check(stmt);
      // Record the statement position right here if the statement is not
      // breakable. For breakable statements the actual recording of the
      // position will be postponed to the breakable code (typically an IC).
      bool position_recorded = CodeGenerator::RecordPositions(
          masm_, stmt->statement_pos(), !checker.is_breakable());
      // If the position recording did record a new position generate a debug
      // break slot to make the statement breakable.
      if (position_recorded) {
        Debug::GenerateSlot(masm_);
      }
    }
#else
    CodeGenerator::RecordPositions(masm_, stmt->statement_pos());
#endif
  }
}


void FullCodeGenerator::SetExpressionPosition(Expression* expr, int pos) {
  if (FLAG_debug_info) {
#ifdef ENABLE_DEBUGGER_SUPPORT
    if (!Debugger::IsDebuggerActive()) {
      CodeGenerator::RecordPositions(masm_, pos);
    } else {
      // Check if the expression will be breakable without adding a debug break
      // slot.
      BreakableStatementChecker checker;
      checker.Check(expr);
      // Record a statement position right here if the expression is not
      // breakable. For breakable expressions the actual recording of the
      // position will be postponed to the breakable code (typically an IC).
      // NOTE this will record a statement position for something which might
      // not be a statement. As stepping in the debugger will only stop at
      // statement positions this is used for e.g. the condition expression of
      // a do while loop.
      bool position_recorded = CodeGenerator::RecordPositions(
          masm_, pos, !checker.is_breakable());
      // If the position recording did record a new position generate a debug
      // break slot to make the statement breakable.
      if (position_recorded) {
        Debug::GenerateSlot(masm_);
      }
    }
#else
    CodeGenerator::RecordPositions(masm_, pos);
#endif
  }
}


void FullCodeGenerator::SetStatementPosition(int pos) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, pos);
  }
}


void FullCodeGenerator::SetSourcePosition(int pos) {
  if (FLAG_debug_info && pos != RelocInfo::kNoPosition) {
    masm_->RecordPosition(pos);
  }
}


void FullCodeGenerator::EmitInlineRuntimeCall(CallRuntime* expr) {
  Handle<String> name = expr->name();
  if (strcmp("_IsSmi", *name->ToCString()) == 0) {
    EmitIsSmi(expr->arguments());
  } else if (strcmp("_IsNonNegativeSmi", *name->ToCString()) == 0) {
    EmitIsNonNegativeSmi(expr->arguments());
  } else if (strcmp("_IsObject", *name->ToCString()) == 0) {
    EmitIsObject(expr->arguments());
  } else if (strcmp("_IsUndetectableObject", *name->ToCString()) == 0) {
    EmitIsUndetectableObject(expr->arguments());
  } else if (strcmp("_IsFunction", *name->ToCString()) == 0) {
    EmitIsFunction(expr->arguments());
  } else if (strcmp("_IsArray", *name->ToCString()) == 0) {
    EmitIsArray(expr->arguments());
  } else if (strcmp("_IsRegExp", *name->ToCString()) == 0) {
    EmitIsRegExp(expr->arguments());
  } else if (strcmp("_IsConstructCall", *name->ToCString()) == 0) {
    EmitIsConstructCall(expr->arguments());
  } else if (strcmp("_ObjectEquals", *name->ToCString()) == 0) {
    EmitObjectEquals(expr->arguments());
  } else if (strcmp("_Arguments", *name->ToCString()) == 0) {
    EmitArguments(expr->arguments());
  } else if (strcmp("_ArgumentsLength", *name->ToCString()) == 0) {
    EmitArgumentsLength(expr->arguments());
  } else if (strcmp("_ClassOf", *name->ToCString()) == 0) {
    EmitClassOf(expr->arguments());
  } else if (strcmp("_Log", *name->ToCString()) == 0) {
    EmitLog(expr->arguments());
  } else if (strcmp("_RandomHeapNumber", *name->ToCString()) == 0) {
    EmitRandomHeapNumber(expr->arguments());
  } else if (strcmp("_SubString", *name->ToCString()) == 0) {
    EmitSubString(expr->arguments());
  } else if (strcmp("_RegExpExec", *name->ToCString()) == 0) {
    EmitRegExpExec(expr->arguments());
  } else if (strcmp("_ValueOf", *name->ToCString()) == 0) {
    EmitValueOf(expr->arguments());
  } else if (strcmp("_SetValueOf", *name->ToCString()) == 0) {
    EmitSetValueOf(expr->arguments());
  } else if (strcmp("_NumberToString", *name->ToCString()) == 0) {
    EmitNumberToString(expr->arguments());
  } else if (strcmp("_StringCharFromCode", *name->ToCString()) == 0) {
    EmitStringCharFromCode(expr->arguments());
  } else if (strcmp("_StringCharCodeAt", *name->ToCString()) == 0) {
    EmitStringCharCodeAt(expr->arguments());
  } else if (strcmp("_StringCharAt", *name->ToCString()) == 0) {
    EmitStringCharAt(expr->arguments());
  } else if (strcmp("_StringAdd", *name->ToCString()) == 0) {
    EmitStringAdd(expr->arguments());
  } else if (strcmp("_StringCompare", *name->ToCString()) == 0) {
    EmitStringCompare(expr->arguments());
  } else if (strcmp("_MathPow", *name->ToCString()) == 0) {
    EmitMathPow(expr->arguments());
  } else if (strcmp("_MathSin", *name->ToCString()) == 0) {
    EmitMathSin(expr->arguments());
  } else if (strcmp("_MathCos", *name->ToCString()) == 0) {
    EmitMathCos(expr->arguments());
  } else if (strcmp("_MathSqrt", *name->ToCString()) == 0) {
    EmitMathSqrt(expr->arguments());
  } else if (strcmp("_CallFunction", *name->ToCString()) == 0) {
    EmitCallFunction(expr->arguments());
  } else if (strcmp("_RegExpConstructResult", *name->ToCString()) == 0) {
    EmitRegExpConstructResult(expr->arguments());
  } else if (strcmp("_SwapElements", *name->ToCString()) == 0) {
    EmitSwapElements(expr->arguments());
  } else if (strcmp("_GetFromCache", *name->ToCString()) == 0) {
    EmitGetFromCache(expr->arguments());
  } else {
    UNREACHABLE();
  }
}


void FullCodeGenerator::EmitLogicalOperation(BinaryOperation* expr) {
  Label eval_right, done;

  // Set up the appropriate context for the left subexpression based
  // on the operation and our own context.  Initially assume we can
  // inherit both true and false labels from our context.
  if (expr->op() == Token::OR) {
    switch (context_) {
      case Expression::kUninitialized:
        UNREACHABLE();
      case Expression::kEffect:
        VisitForControl(expr->left(), &done, &eval_right);
        break;
      case Expression::kValue:
        VisitForValueControl(expr->left(),
                             location_,
                             &done,
                             &eval_right);
        break;
      case Expression::kTest:
        VisitForControl(expr->left(), true_label_, &eval_right);
        break;
      case Expression::kValueTest:
        VisitForValueControl(expr->left(),
                             location_,
                             true_label_,
                             &eval_right);
        break;
      case Expression::kTestValue:
        VisitForControl(expr->left(), true_label_, &eval_right);
        break;
    }
  } else {
    ASSERT_EQ(Token::AND, expr->op());
    switch (context_) {
      case Expression::kUninitialized:
        UNREACHABLE();
      case Expression::kEffect:
        VisitForControl(expr->left(), &eval_right, &done);
        break;
      case Expression::kValue:
        VisitForControlValue(expr->left(),
                             location_,
                             &eval_right,
                             &done);
        break;
      case Expression::kTest:
        VisitForControl(expr->left(), &eval_right, false_label_);
        break;
      case Expression::kValueTest:
        VisitForControl(expr->left(), &eval_right, false_label_);
        break;
      case Expression::kTestValue:
        VisitForControlValue(expr->left(),
                             location_,
                             &eval_right,
                             false_label_);
        break;
    }
  }

  __ bind(&eval_right);
  Visit(expr->right());

  __ bind(&done);
}


void FullCodeGenerator::VisitBlock(Block* stmt) {
  Comment cmnt(masm_, "[ Block");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);
  VisitStatements(stmt->statements());
  __ bind(nested_statement.break_target());
}


void FullCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Comment cmnt(masm_, "[ ExpressionStatement");
  SetStatementPosition(stmt);
  VisitForEffect(stmt->expression());
}


void FullCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  Comment cmnt(masm_, "[ EmptyStatement");
  SetStatementPosition(stmt);
}


void FullCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  Comment cmnt(masm_, "[ IfStatement");
  SetStatementPosition(stmt);
  Label then_part, else_part, done;

  // Do not worry about optimizing for empty then or else bodies.
  VisitForControl(stmt->condition(), &then_part, &else_part);

  __ bind(&then_part);
  Visit(stmt->then_statement());
  __ jmp(&done);

  __ bind(&else_part);
  Visit(stmt->else_statement());

  __ bind(&done);
}


void FullCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  Comment cmnt(masm_,  "[ ContinueStatement");
  SetStatementPosition(stmt);
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  while (!current->IsContinueTarget(stmt->target())) {
    stack_depth = current->Exit(stack_depth);
    current = current->outer();
  }
  __ Drop(stack_depth);

  Iteration* loop = current->AsIteration();
  __ jmp(loop->continue_target());
}


void FullCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  Comment cmnt(masm_,  "[ BreakStatement");
  SetStatementPosition(stmt);
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  while (!current->IsBreakTarget(stmt->target())) {
    stack_depth = current->Exit(stack_depth);
    current = current->outer();
  }
  __ Drop(stack_depth);

  Breakable* target = current->AsBreakable();
  __ jmp(target->break_target());
}


void FullCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  SetStatementPosition(stmt);
  Expression* expr = stmt->expression();
  VisitForValue(expr, kAccumulator);

  // Exit all nested statements.
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  while (current != NULL) {
    stack_depth = current->Exit(stack_depth);
    current = current->outer();
  }
  __ Drop(stack_depth);

  EmitReturnSequence(stmt->statement_pos());
}


void FullCodeGenerator::VisitWithEnterStatement(WithEnterStatement* stmt) {
  Comment cmnt(masm_, "[ WithEnterStatement");
  SetStatementPosition(stmt);

  VisitForValue(stmt->expression(), kStack);
  if (stmt->is_catch_block()) {
    __ CallRuntime(Runtime::kPushCatchContext, 1);
  } else {
    __ CallRuntime(Runtime::kPushContext, 1);
  }
  // Both runtime calls return the new context in both the context and the
  // result registers.

  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
}


void FullCodeGenerator::VisitWithExitStatement(WithExitStatement* stmt) {
  Comment cmnt(masm_, "[ WithExitStatement");
  SetStatementPosition(stmt);

  // Pop context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
}


void FullCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  Comment cmnt(masm_, "[ DoWhileStatement");
  SetStatementPosition(stmt);
  Label body, stack_limit_hit, stack_check_success;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  __ bind(&body);
  Visit(stmt->body());

  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  __ bind(loop_statement.continue_target());

  // Record the position of the do while condition and make sure it is possible
  // to break on the condition.
  SetExpressionPosition(stmt->cond(), stmt->condition_position());

  VisitForControl(stmt->cond(), &body, loop_statement.break_target());

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(loop_statement.break_target());

  decrement_loop_depth();
}


void FullCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  Comment cmnt(masm_, "[ WhileStatement");
  Label body, stack_limit_hit, stack_check_success;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // Emit the test at the bottom of the loop.
  __ jmp(loop_statement.continue_target());

  __ bind(&body);
  Visit(stmt->body());

  __ bind(loop_statement.continue_target());
  // Emit the statement position here as this is where the while statement code
  // starts.
  SetStatementPosition(stmt);

  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  VisitForControl(stmt->cond(), &body, loop_statement.break_target());

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(loop_statement.break_target());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitForStatement(ForStatement* stmt) {
  Comment cmnt(masm_, "[ ForStatement");
  Label test, body, stack_limit_hit, stack_check_success;

  Iteration loop_statement(this, stmt);
  if (stmt->init() != NULL) {
    Visit(stmt->init());
  }

  increment_loop_depth();
  // Emit the test at the bottom of the loop (even if empty).
  __ jmp(&test);

  __ bind(&body);
  Visit(stmt->body());

  __ bind(loop_statement.continue_target());

  SetStatementPosition(stmt);
  if (stmt->next() != NULL) {
    Visit(stmt->next());
  }

  __ bind(&test);
  // Emit the statement position here as this is where the for statement code
  // starts.
  SetStatementPosition(stmt);

  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  if (stmt->cond() != NULL) {
    VisitForControl(stmt->cond(), &body, loop_statement.break_target());
  } else {
    __ jmp(&body);
  }

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(loop_statement.break_target());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Comment cmnt(masm_, "[ TryCatchStatement");
  SetStatementPosition(stmt);
  // The try block adds a handler to the exception handler chain
  // before entering, and removes it again when exiting normally.
  // If an exception is thrown during execution of the try block,
  // control is passed to the handler, which also consumes the handler.
  // At this point, the exception is in a register, and store it in
  // the temporary local variable (prints as ".catch-var") before
  // executing the catch block. The catch block has been rewritten
  // to introduce a new scope to bind the catch variable and to remove
  // that scope again afterwards.

  Label try_handler_setup, catch_entry, done;
  __ Call(&try_handler_setup);
  // Try handler code, exception in result register.

  // Store exception in local .catch variable before executing catch block.
  {
    // The catch variable is *always* a variable proxy for a local variable.
    Variable* catch_var = stmt->catch_var()->AsVariableProxy()->AsVariable();
    ASSERT_NOT_NULL(catch_var);
    Slot* variable_slot = catch_var->slot();
    ASSERT_NOT_NULL(variable_slot);
    ASSERT_EQ(Slot::LOCAL, variable_slot->type());
    StoreToFrameField(SlotOffset(variable_slot), result_register());
  }

  Visit(stmt->catch_block());
  __ jmp(&done);

  // Try block code. Sets up the exception handler chain.
  __ bind(&try_handler_setup);
  {
    TryCatch try_block(this, &catch_entry);
    __ PushTryHandler(IN_JAVASCRIPT, TRY_CATCH_HANDLER);
    Visit(stmt->try_block());
    __ PopTryHandler();
  }
  __ bind(&done);
}


void FullCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  Comment cmnt(masm_, "[ TryFinallyStatement");
  SetStatementPosition(stmt);
  // Try finally is compiled by setting up a try-handler on the stack while
  // executing the try body, and removing it again afterwards.
  //
  // The try-finally construct can enter the finally block in three ways:
  // 1. By exiting the try-block normally. This removes the try-handler and
  //      calls the finally block code before continuing.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (break/continue/return). The site of the, e.g., break removes the
  //    try handler and calls the finally block code before continuing
  //    its outward control transfer.
  // 3. by exiting the try-block with a thrown exception.
  //    This can happen in nested function calls. It traverses the try-handler
  //    chain and consumes the try-handler entry before jumping to the
  //    handler code. The handler code then calls the finally-block before
  //    rethrowing the exception.
  //
  // The finally block must assume a return address on top of the stack
  // (or in the link register on ARM chips) and a value (return value or
  // exception) in the result register (rax/eax/r0), both of which must
  // be preserved. The return address isn't GC-safe, so it should be
  // cooked before GC.
  Label finally_entry;
  Label try_handler_setup;

  // Setup the try-handler chain. Use a call to
  // Jump to try-handler setup and try-block code. Use call to put try-handler
  // address on stack.
  __ Call(&try_handler_setup);
  // Try handler code. Return address of call is pushed on handler stack.
  {
    // This code is only executed during stack-handler traversal when an
    // exception is thrown. The execption is in the result register, which
    // is retained by the finally block.
    // Call the finally block and then rethrow the exception.
    __ Call(&finally_entry);
    __ push(result_register());
    __ CallRuntime(Runtime::kReThrow, 1);
  }

  __ bind(&finally_entry);
  {
    // Finally block implementation.
    Finally finally_block(this);
    EnterFinallyBlock();
    Visit(stmt->finally_block());
    ExitFinallyBlock();  // Return to the calling code.
  }

  __ bind(&try_handler_setup);
  {
    // Setup try handler (stack pointer registers).
    TryFinally try_block(this, &finally_entry);
    __ PushTryHandler(IN_JAVASCRIPT, TRY_FINALLY_HANDLER);
    Visit(stmt->try_block());
    __ PopTryHandler();
  }
  // Execute the finally block on the way out.
  __ Call(&finally_entry);
}


void FullCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  Comment cmnt(masm_, "[ DebuggerStatement");
  SetStatementPosition(stmt);

  __ DebugBreak();
  // Ignore the return value.
#endif
}


void FullCodeGenerator::VisitConditional(Conditional* expr) {
  Comment cmnt(masm_, "[ Conditional");
  Label true_case, false_case, done;
  VisitForControl(expr->condition(), &true_case, &false_case);

  __ bind(&true_case);
  SetExpressionPosition(expr->then_expression(),
                        expr->then_expression_position());
  Visit(expr->then_expression());
  // If control flow falls through Visit, jump to done.
  if (context_ == Expression::kEffect || context_ == Expression::kValue) {
    __ jmp(&done);
  }

  __ bind(&false_case);
  SetExpressionPosition(expr->else_expression(),
                        expr->else_expression_position());
  Visit(expr->else_expression());
  // If control flow falls through Visit, merge it with true case here.
  if (context_ == Expression::kEffect || context_ == Expression::kValue) {
    __ bind(&done);
  }
}


void FullCodeGenerator::VisitSlot(Slot* expr) {
  // Slots do not appear directly in the AST.
  UNREACHABLE();
}


void FullCodeGenerator::VisitLiteral(Literal* expr) {
  Comment cmnt(masm_, "[ Literal");
  Apply(context_, expr);
}


void FullCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<SharedFunctionInfo> function_info =
      Compiler::BuildFunctionInfo(expr, script(), this);
  if (HasStackOverflow()) return;
  EmitNewClosure(function_info);
}


void FullCodeGenerator::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  Comment cmnt(masm_, "[ SharedFunctionInfoLiteral");
  EmitNewClosure(expr->shared_function_info());
}


void FullCodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  // Call runtime routine to allocate the catch extension object and
  // assign the exception value to the catch variable.
  Comment cmnt(masm_, "[ CatchExtensionObject");
  VisitForValue(expr->key(), kStack);
  VisitForValue(expr->value(), kStack);
  // Create catch extension object.
  __ CallRuntime(Runtime::kCreateCatchExtensionObject, 2);
  Apply(context_, result_register());
}


void FullCodeGenerator::VisitThrow(Throw* expr) {
  Comment cmnt(masm_, "[ Throw");
  VisitForValue(expr->exception(), kStack);
  __ CallRuntime(Runtime::kThrow, 1);
  // Never returns here.
}


int FullCodeGenerator::TryFinally::Exit(int stack_depth) {
  // The macros used here must preserve the result register.
  __ Drop(stack_depth);
  __ PopTryHandler();
  __ Call(finally_entry_);
  return 0;
}


int FullCodeGenerator::TryCatch::Exit(int stack_depth) {
  // The macros used here must preserve the result register.
  __ Drop(stack_depth);
  __ PopTryHandler();
  return 0;
}

#undef __


} }  // namespace v8::internal
