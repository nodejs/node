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

#include "codegen-inl.h"
#include "data-flow.h"
#include "fast-codegen.h"
#include "scopes.h"

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


void FastCodeGenSyntaxChecker::Check(FunctionLiteral* fun,
                                     CompilationInfo* info) {
  info_ = info;

  // We do not specialize if we do not have a receiver or if it is not a
  // JS object with fast mode properties.
  if (!info->has_receiver()) BAILOUT("No receiver");
  if (!info->receiver()->IsJSObject()) BAILOUT("Receiver is not an object");
  Handle<JSObject> object = Handle<JSObject>::cast(info->receiver());
  if (!object->HasFastProperties()) BAILOUT("Receiver is in dictionary mode");

  // We do not support stack or heap slots (both of which require
  // allocation).
  Scope* scope = fun->scope();
  if (scope->num_stack_slots() > 0) {
    BAILOUT("Function has stack-allocated locals");
  }
  if (scope->num_heap_slots() > 0) {
    BAILOUT("Function has context-allocated locals");
  }

  VisitDeclarations(scope->declarations());
  CHECK_BAILOUT;

  // We do not support empty function bodies.
  if (fun->body()->is_empty()) BAILOUT("Function has an empty body");
  VisitStatements(fun->body());
}


void FastCodeGenSyntaxChecker::VisitDeclarations(
    ZoneList<Declaration*>* decls) {
  if (!decls->is_empty()) BAILOUT("Function has declarations");
}


void FastCodeGenSyntaxChecker::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
    CHECK_BAILOUT;
  }
}


void FastCodeGenSyntaxChecker::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void FastCodeGenSyntaxChecker::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void FastCodeGenSyntaxChecker::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void FastCodeGenSyntaxChecker::VisitEmptyStatement(EmptyStatement* stmt) {
  // Supported.
}


void FastCodeGenSyntaxChecker::VisitIfStatement(IfStatement* stmt) {
  BAILOUT("IfStatement");
}


void FastCodeGenSyntaxChecker::VisitContinueStatement(ContinueStatement* stmt) {
  BAILOUT("Continuestatement");
}


void FastCodeGenSyntaxChecker::VisitBreakStatement(BreakStatement* stmt) {
  BAILOUT("BreakStatement");
}


void FastCodeGenSyntaxChecker::VisitReturnStatement(ReturnStatement* stmt) {
  BAILOUT("ReturnStatement");
}


void FastCodeGenSyntaxChecker::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  BAILOUT("WithEnterStatement");
}


void FastCodeGenSyntaxChecker::VisitWithExitStatement(WithExitStatement* stmt) {
  BAILOUT("WithExitStatement");
}


void FastCodeGenSyntaxChecker::VisitSwitchStatement(SwitchStatement* stmt) {
  BAILOUT("SwitchStatement");
}


void FastCodeGenSyntaxChecker::VisitDoWhileStatement(DoWhileStatement* stmt) {
  BAILOUT("DoWhileStatement");
}


void FastCodeGenSyntaxChecker::VisitWhileStatement(WhileStatement* stmt) {
  BAILOUT("WhileStatement");
}


void FastCodeGenSyntaxChecker::VisitForStatement(ForStatement* stmt) {
  BAILOUT("ForStatement");
}


void FastCodeGenSyntaxChecker::VisitForInStatement(ForInStatement* stmt) {
  BAILOUT("ForInStatement");
}


void FastCodeGenSyntaxChecker::VisitTryCatchStatement(TryCatchStatement* stmt) {
  BAILOUT("TryCatchStatement");
}


void FastCodeGenSyntaxChecker::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  BAILOUT("TryFinallyStatement");
}


void FastCodeGenSyntaxChecker::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  BAILOUT("DebuggerStatement");
}


void FastCodeGenSyntaxChecker::VisitFunctionLiteral(FunctionLiteral* expr) {
  BAILOUT("FunctionLiteral");
}


void FastCodeGenSyntaxChecker::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  BAILOUT("FunctionBoilerplateLiteral");
}


void FastCodeGenSyntaxChecker::VisitConditional(Conditional* expr) {
  BAILOUT("Conditional");
}


void FastCodeGenSyntaxChecker::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void FastCodeGenSyntaxChecker::VisitVariableProxy(VariableProxy* expr) {
  // Only global variable references are supported.
  Variable* var = expr->var();
  if (!var->is_global()) BAILOUT("Non-global variable");
}


void FastCodeGenSyntaxChecker::VisitLiteral(Literal* expr) {
  BAILOUT("Literal");
}


void FastCodeGenSyntaxChecker::VisitRegExpLiteral(RegExpLiteral* expr) {
  BAILOUT("RegExpLiteral");
}


void FastCodeGenSyntaxChecker::VisitObjectLiteral(ObjectLiteral* expr) {
  BAILOUT("ObjectLiteral");
}


void FastCodeGenSyntaxChecker::VisitArrayLiteral(ArrayLiteral* expr) {
  BAILOUT("ArrayLiteral");
}


void FastCodeGenSyntaxChecker::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  BAILOUT("CatchExtensionObject");
}


void FastCodeGenSyntaxChecker::VisitAssignment(Assignment* expr) {
  // Simple assignments to (named) this properties are supported.
  if (expr->op() != Token::ASSIGN) BAILOUT("Non-simple assignment");

  Property* prop = expr->target()->AsProperty();
  if (prop == NULL) BAILOUT("Non-property assignment");
  VariableProxy* proxy = prop->obj()->AsVariableProxy();
  if (proxy == NULL || !proxy->var()->is_this()) {
    BAILOUT("Non-this-property assignment");
  }
  if (!prop->key()->IsPropertyName()) {
    BAILOUT("Non-named-property assignment");
  }

  // We will only specialize for fields on the object itself.
  // Expression::IsPropertyName implies that the name is a literal
  // symbol but we do not assume that.
  Literal* key = prop->key()->AsLiteral();
  if (key != NULL && key->handle()->IsString()) {
    Handle<Object> receiver = info()->receiver();
    Handle<String> name = Handle<String>::cast(key->handle());
    LookupResult lookup;
    receiver->Lookup(*name, &lookup);
    if (lookup.holder() != *receiver) BAILOUT("Non-own property assignment");
    if (!lookup.type() == FIELD) BAILOUT("Non-field property assignment");
  } else {
    UNREACHABLE();
    BAILOUT("Unexpected non-string-literal property key");
  }

  Visit(expr->value());
}


void FastCodeGenSyntaxChecker::VisitThrow(Throw* expr) {
  BAILOUT("Throw");
}


void FastCodeGenSyntaxChecker::VisitProperty(Property* expr) {
  BAILOUT("Property");
}


void FastCodeGenSyntaxChecker::VisitCall(Call* expr) {
  BAILOUT("Call");
}


void FastCodeGenSyntaxChecker::VisitCallNew(CallNew* expr) {
  BAILOUT("CallNew");
}


void FastCodeGenSyntaxChecker::VisitCallRuntime(CallRuntime* expr) {
  BAILOUT("CallRuntime");
}


void FastCodeGenSyntaxChecker::VisitUnaryOperation(UnaryOperation* expr) {
  BAILOUT("UnaryOperation");
}


void FastCodeGenSyntaxChecker::VisitCountOperation(CountOperation* expr) {
  BAILOUT("CountOperation");
}


void FastCodeGenSyntaxChecker::VisitBinaryOperation(BinaryOperation* expr) {
  BAILOUT("BinaryOperation");
}


void FastCodeGenSyntaxChecker::VisitCompareOperation(CompareOperation* expr) {
  BAILOUT("CompareOperation");
}


void FastCodeGenSyntaxChecker::VisitThisFunction(ThisFunction* expr) {
  BAILOUT("ThisFunction");
}

#undef BAILOUT
#undef CHECK_BAILOUT


#define __ ACCESS_MASM(masm())

Handle<Code> FastCodeGenerator::MakeCode(FunctionLiteral* fun,
                                         Handle<Script> script,
                                         bool is_eval,
                                         CompilationInfo* info) {
  // Label the AST before calling MakeCodePrologue, so AST node numbers are
  // printed with the AST.
  AstLabeler labeler;
  labeler.Label(fun);
  info->set_has_this_properties(labeler.has_this_properties());

  CodeGenerator::MakeCodePrologue(fun);

  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(NULL, kInitialBufferSize);

  // Generate the fast-path code.
  FastCodeGenerator fast_cgen(&masm, script, is_eval);
  fast_cgen.Generate(fun, info);
  if (fast_cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  // Generate the full code for the function in bailout mode, using the same
  // macro assembler.
  CodeGenerator cgen(&masm, script, is_eval);
  CodeGeneratorScope scope(&cgen);
  cgen.Generate(fun, CodeGenerator::SECONDARY, info);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, NOT_IN_LOOP);
  return CodeGenerator::MakeCodeEpilogue(fun, &masm, flags, script);
}


void FastCodeGenerator::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void FastCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void FastCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  // Nothing to do.
}


void FastCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithEnterStatement(WithEnterStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  ASSERT(expr->var()->is_global() && !expr->var()->is_this());
  Comment cmnt(masm(), ";; Global");
  if (FLAG_print_ir) {
    SmartPointer<char> name = expr->name()->ToCString();
    PrintF("%d: t%d = Global(%s)\n", expr->num(), expr->num(), *name);
  }
  EmitGlobalVariableLoad(expr->name());
}


void FastCodeGenerator::VisitLiteral(Literal* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitAssignment(Assignment* expr) {
  // Known to be a simple this property assignment.
  Visit(expr->value());

  Property* prop = expr->target()->AsProperty();
  ASSERT_NOT_NULL(prop);
  ASSERT_NOT_NULL(prop->obj()->AsVariableProxy());
  ASSERT(prop->obj()->AsVariableProxy()->var()->is_this());
  ASSERT(prop->key()->IsPropertyName());
  Handle<String> name =
      Handle<String>::cast(prop->key()->AsLiteral()->handle());

  Comment cmnt(masm(), ";; Store(this)");
  if (FLAG_print_ir) {
    SmartPointer<char> name_string = name->ToCString();
    PrintF("%d: t%d = Store(this, \"%s\", t%d)\n",
           expr->num(), expr->num(), *name_string, expr->value()->num());
  }

  EmitThisPropertyStore(name);
}


void FastCodeGenerator::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitProperty(Property* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCall(Call* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}

#undef __


} }  // namespace v8::internal
