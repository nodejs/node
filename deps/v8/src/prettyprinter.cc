// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>

#include "src/v8.h"

#include "src/ast-value-factory.h"
#include "src/base/platform/platform.h"
#include "src/prettyprinter.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {

CallPrinter::CallPrinter(Isolate* isolate, Zone* zone) {
  output_ = NULL;
  size_ = 0;
  pos_ = 0;
  position_ = 0;
  found_ = false;
  done_ = false;
  InitializeAstVisitor(isolate, zone);
}


CallPrinter::~CallPrinter() { DeleteArray(output_); }


const char* CallPrinter::Print(FunctionLiteral* program, int position) {
  Init();
  position_ = position;
  Find(program);
  return output_;
}


void CallPrinter::Find(AstNode* node, bool print) {
  if (done_) return;
  if (found_) {
    if (print) {
      int start = pos_;
      Visit(node);
      if (start != pos_) return;
    }
    Print("(intermediate value)");
  } else {
    Visit(node);
  }
}


void CallPrinter::Init() {
  if (size_ == 0) {
    DCHECK(output_ == NULL);
    const int initial_size = 256;
    output_ = NewArray<char>(initial_size);
    size_ = initial_size;
  }
  output_[0] = '\0';
  pos_ = 0;
}


void CallPrinter::Print(const char* format, ...) {
  if (!found_ || done_) return;
  for (;;) {
    va_list arguments;
    va_start(arguments, format);
    int n = VSNPrintF(Vector<char>(output_, size_) + pos_, format, arguments);
    va_end(arguments);

    if (n >= 0) {
      // there was enough space - we are done
      pos_ += n;
      return;
    } else {
      // there was not enough space - allocate more and try again
      const int slack = 32;
      int new_size = size_ + (size_ >> 1) + slack;
      char* new_output = NewArray<char>(new_size);
      MemCopy(new_output, output_, pos_);
      DeleteArray(output_);
      output_ = new_output;
      size_ = new_size;
    }
  }
}


void CallPrinter::VisitBlock(Block* node) {
  FindStatements(node->statements());
}


void CallPrinter::VisitVariableDeclaration(VariableDeclaration* node) {}


void CallPrinter::VisitFunctionDeclaration(FunctionDeclaration* node) {}


void CallPrinter::VisitModuleDeclaration(ModuleDeclaration* node) {
  Find(node->module());
}


void CallPrinter::VisitImportDeclaration(ImportDeclaration* node) {
}


void CallPrinter::VisitExportDeclaration(ExportDeclaration* node) {}


void CallPrinter::VisitModuleLiteral(ModuleLiteral* node) {
  VisitBlock(node->body());
}


void CallPrinter::VisitModulePath(ModulePath* node) { Find(node->module()); }


void CallPrinter::VisitModuleUrl(ModuleUrl* node) {}


void CallPrinter::VisitModuleStatement(ModuleStatement* node) {
  Find(node->body());
}


void CallPrinter::VisitExpressionStatement(ExpressionStatement* node) {
  Find(node->expression());
}


void CallPrinter::VisitEmptyStatement(EmptyStatement* node) {}


void CallPrinter::VisitIfStatement(IfStatement* node) {
  Find(node->condition());
  Find(node->then_statement());
  if (node->HasElseStatement()) {
    Find(node->else_statement());
  }
}


void CallPrinter::VisitContinueStatement(ContinueStatement* node) {}


void CallPrinter::VisitBreakStatement(BreakStatement* node) {}


void CallPrinter::VisitReturnStatement(ReturnStatement* node) {
  Find(node->expression());
}


void CallPrinter::VisitWithStatement(WithStatement* node) {
  Find(node->expression());
  Find(node->statement());
}


void CallPrinter::VisitSwitchStatement(SwitchStatement* node) {
  Find(node->tag());
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = 0; i < cases->length(); i++) Find(cases->at(i));
}


void CallPrinter::VisitCaseClause(CaseClause* clause) {
  if (!clause->is_default()) {
    Find(clause->label());
  }
  FindStatements(clause->statements());
}


void CallPrinter::VisitDoWhileStatement(DoWhileStatement* node) {
  Find(node->body());
  Find(node->cond());
}


void CallPrinter::VisitWhileStatement(WhileStatement* node) {
  Find(node->cond());
  Find(node->body());
}


void CallPrinter::VisitForStatement(ForStatement* node) {
  if (node->init() != NULL) {
    Find(node->init());
  }
  if (node->cond() != NULL) Find(node->cond());
  if (node->next() != NULL) Find(node->next());
  Find(node->body());
}


void CallPrinter::VisitForInStatement(ForInStatement* node) {
  Find(node->each());
  Find(node->enumerable());
  Find(node->body());
}


void CallPrinter::VisitForOfStatement(ForOfStatement* node) {
  Find(node->each());
  Find(node->iterable());
  Find(node->body());
}


void CallPrinter::VisitTryCatchStatement(TryCatchStatement* node) {
  Find(node->try_block());
  Find(node->catch_block());
}


void CallPrinter::VisitTryFinallyStatement(TryFinallyStatement* node) {
  Find(node->try_block());
  Find(node->finally_block());
}


void CallPrinter::VisitDebuggerStatement(DebuggerStatement* node) {}


void CallPrinter::VisitFunctionLiteral(FunctionLiteral* node) {
  FindStatements(node->body());
}


void CallPrinter::VisitClassLiteral(ClassLiteral* node) {
  if (node->extends()) Find(node->extends());
  for (int i = 0; i < node->properties()->length(); i++) {
    Find(node->properties()->at(i)->value());
  }
}


void CallPrinter::VisitNativeFunctionLiteral(NativeFunctionLiteral* node) {}


void CallPrinter::VisitConditional(Conditional* node) {
  Find(node->condition());
  Find(node->then_expression());
  Find(node->else_expression());
}


void CallPrinter::VisitLiteral(Literal* node) {
  PrintLiteral(node->value(), true);
}


void CallPrinter::VisitRegExpLiteral(RegExpLiteral* node) {
  Print("/");
  PrintLiteral(node->pattern(), false);
  Print("/");
  PrintLiteral(node->flags(), false);
}


void CallPrinter::VisitObjectLiteral(ObjectLiteral* node) {
  for (int i = 0; i < node->properties()->length(); i++) {
    Find(node->properties()->at(i)->value());
  }
}


void CallPrinter::VisitArrayLiteral(ArrayLiteral* node) {
  Print("[");
  for (int i = 0; i < node->values()->length(); i++) {
    if (i != 0) Print(",");
    Find(node->values()->at(i), true);
  }
  Print("]");
}


void CallPrinter::VisitVariableProxy(VariableProxy* node) {
  PrintLiteral(node->name(), false);
}


void CallPrinter::VisitAssignment(Assignment* node) {
  Find(node->target());
  Find(node->value());
}


void CallPrinter::VisitYield(Yield* node) { Find(node->expression()); }


void CallPrinter::VisitThrow(Throw* node) { Find(node->exception()); }


void CallPrinter::VisitProperty(Property* node) {
  Expression* key = node->key();
  Literal* literal = key->AsLiteral();
  if (literal != NULL && literal->value()->IsInternalizedString()) {
    Find(node->obj(), true);
    Print(".");
    PrintLiteral(literal->value(), false);
  } else {
    Find(node->obj(), true);
    Print("[");
    Find(key, true);
    Print("]");
  }
}


void CallPrinter::VisitCall(Call* node) {
  bool was_found = !found_ && node->position() == position_;
  if (was_found) found_ = true;
  Find(node->expression(), true);
  if (!was_found) Print("(...)");
  FindArguments(node->arguments());
  if (was_found) done_ = true;
}


void CallPrinter::VisitCallNew(CallNew* node) {
  bool was_found = !found_ && node->expression()->position() == position_;
  if (was_found) found_ = true;
  Find(node->expression(), was_found);
  FindArguments(node->arguments());
  if (was_found) done_ = true;
}


void CallPrinter::VisitCallRuntime(CallRuntime* node) {
  FindArguments(node->arguments());
}


void CallPrinter::VisitUnaryOperation(UnaryOperation* node) {
  Token::Value op = node->op();
  bool needsSpace =
      op == Token::DELETE || op == Token::TYPEOF || op == Token::VOID;
  Print("(%s%s", Token::String(op), needsSpace ? " " : "");
  Find(node->expression(), true);
  Print(")");
}


void CallPrinter::VisitCountOperation(CountOperation* node) {
  Print("(");
  if (node->is_prefix()) Print("%s", Token::String(node->op()));
  Find(node->expression(), true);
  if (node->is_postfix()) Print("%s", Token::String(node->op()));
  Print(")");
}


void CallPrinter::VisitBinaryOperation(BinaryOperation* node) {
  Print("(");
  Find(node->left(), true);
  Print(" %s ", Token::String(node->op()));
  Find(node->right(), true);
  Print(")");
}


void CallPrinter::VisitCompareOperation(CompareOperation* node) {
  Print("(");
  Find(node->left(), true);
  Print(" %s ", Token::String(node->op()));
  Find(node->right(), true);
  Print(")");
}


void CallPrinter::VisitThisFunction(ThisFunction* node) {}


void CallPrinter::VisitSuperReference(SuperReference* node) {}


void CallPrinter::FindStatements(ZoneList<Statement*>* statements) {
  if (statements == NULL) return;
  for (int i = 0; i < statements->length(); i++) {
    Find(statements->at(i));
  }
}


void CallPrinter::FindArguments(ZoneList<Expression*>* arguments) {
  if (found_) return;
  for (int i = 0; i < arguments->length(); i++) {
    Find(arguments->at(i));
  }
}


void CallPrinter::PrintLiteral(Handle<Object> value, bool quote) {
  Object* object = *value;
  if (object->IsString()) {
    String* string = String::cast(object);
    if (quote) Print("\"");
    for (int i = 0; i < string->length(); i++) {
      Print("%c", string->Get(i));
    }
    if (quote) Print("\"");
  } else if (object->IsNull()) {
    Print("null");
  } else if (object->IsTrue()) {
    Print("true");
  } else if (object->IsFalse()) {
    Print("false");
  } else if (object->IsUndefined()) {
    Print("undefined");
  } else if (object->IsNumber()) {
    Print("%g", object->Number());
  }
}


void CallPrinter::PrintLiteral(const AstRawString* value, bool quote) {
  PrintLiteral(value->string(), quote);
}


//-----------------------------------------------------------------------------


#ifdef DEBUG

PrettyPrinter::PrettyPrinter(Isolate* isolate, Zone* zone) {
  output_ = NULL;
  size_ = 0;
  pos_ = 0;
  InitializeAstVisitor(isolate, zone);
}


PrettyPrinter::~PrettyPrinter() {
  DeleteArray(output_);
}


void PrettyPrinter::VisitBlock(Block* node) {
  if (!node->is_initializer_block()) Print("{ ");
  PrintStatements(node->statements());
  if (node->statements()->length() > 0) Print(" ");
  if (!node->is_initializer_block()) Print("}");
}


void PrettyPrinter::VisitVariableDeclaration(VariableDeclaration* node) {
  Print("var ");
  PrintLiteral(node->proxy()->name(), false);
  Print(";");
}


void PrettyPrinter::VisitFunctionDeclaration(FunctionDeclaration* node) {
  Print("function ");
  PrintLiteral(node->proxy()->name(), false);
  Print(" = ");
  PrintFunctionLiteral(node->fun());
  Print(";");
}


void PrettyPrinter::VisitModuleDeclaration(ModuleDeclaration* node) {
  Print("module ");
  PrintLiteral(node->proxy()->name(), false);
  Print(" = ");
  Visit(node->module());
  Print(";");
}


void PrettyPrinter::VisitImportDeclaration(ImportDeclaration* node) {
  Print("import ");
  PrintLiteral(node->proxy()->name(), false);
  Print(" from ");
  PrintLiteral(node->module_specifier()->string(), true);
  Print(";");
}


void PrettyPrinter::VisitExportDeclaration(ExportDeclaration* node) {
  Print("export ");
  PrintLiteral(node->proxy()->name(), false);
  Print(";");
}


void PrettyPrinter::VisitModuleLiteral(ModuleLiteral* node) {
  VisitBlock(node->body());
}


void PrettyPrinter::VisitModulePath(ModulePath* node) {
  Visit(node->module());
  Print(".");
  PrintLiteral(node->name(), false);
}


void PrettyPrinter::VisitModuleUrl(ModuleUrl* node) {
  Print("at ");
  PrintLiteral(node->url(), true);
}


void PrettyPrinter::VisitModuleStatement(ModuleStatement* node) {
  Print("module ");
  Visit(node->body());
}


void PrettyPrinter::VisitExpressionStatement(ExpressionStatement* node) {
  Visit(node->expression());
  Print(";");
}


void PrettyPrinter::VisitEmptyStatement(EmptyStatement* node) {
  Print(";");
}


void PrettyPrinter::VisitIfStatement(IfStatement* node) {
  Print("if (");
  Visit(node->condition());
  Print(") ");
  Visit(node->then_statement());
  if (node->HasElseStatement()) {
    Print(" else ");
    Visit(node->else_statement());
  }
}


void PrettyPrinter::VisitContinueStatement(ContinueStatement* node) {
  Print("continue");
  ZoneList<const AstRawString*>* labels = node->target()->labels();
  if (labels != NULL) {
    Print(" ");
    DCHECK(labels->length() > 0);  // guaranteed to have at least one entry
    PrintLiteral(labels->at(0), false);  // any label from the list is fine
  }
  Print(";");
}


void PrettyPrinter::VisitBreakStatement(BreakStatement* node) {
  Print("break");
  ZoneList<const AstRawString*>* labels = node->target()->labels();
  if (labels != NULL) {
    Print(" ");
    DCHECK(labels->length() > 0);  // guaranteed to have at least one entry
    PrintLiteral(labels->at(0), false);  // any label from the list is fine
  }
  Print(";");
}


void PrettyPrinter::VisitReturnStatement(ReturnStatement* node) {
  Print("return ");
  Visit(node->expression());
  Print(";");
}


void PrettyPrinter::VisitWithStatement(WithStatement* node) {
  Print("with (");
  Visit(node->expression());
  Print(") ");
  Visit(node->statement());
}


void PrettyPrinter::VisitSwitchStatement(SwitchStatement* node) {
  PrintLabels(node->labels());
  Print("switch (");
  Visit(node->tag());
  Print(") { ");
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = 0; i < cases->length(); i++)
    Visit(cases->at(i));
  Print("}");
}


void PrettyPrinter::VisitCaseClause(CaseClause* clause) {
  if (clause->is_default()) {
    Print("default");
  } else {
    Print("case ");
    Visit(clause->label());
  }
  Print(": ");
  PrintStatements(clause->statements());
  if (clause->statements()->length() > 0)
    Print(" ");
}


void PrettyPrinter::VisitDoWhileStatement(DoWhileStatement* node) {
  PrintLabels(node->labels());
  Print("do ");
  Visit(node->body());
  Print(" while (");
  Visit(node->cond());
  Print(");");
}


void PrettyPrinter::VisitWhileStatement(WhileStatement* node) {
  PrintLabels(node->labels());
  Print("while (");
  Visit(node->cond());
  Print(") ");
  Visit(node->body());
}


void PrettyPrinter::VisitForStatement(ForStatement* node) {
  PrintLabels(node->labels());
  Print("for (");
  if (node->init() != NULL) {
    Visit(node->init());
    Print(" ");
  } else {
    Print("; ");
  }
  if (node->cond() != NULL) Visit(node->cond());
  Print("; ");
  if (node->next() != NULL) {
    Visit(node->next());  // prints extra ';', unfortunately
    // to fix: should use Expression for next
  }
  Print(") ");
  Visit(node->body());
}


void PrettyPrinter::VisitForInStatement(ForInStatement* node) {
  PrintLabels(node->labels());
  Print("for (");
  Visit(node->each());
  Print(" in ");
  Visit(node->enumerable());
  Print(") ");
  Visit(node->body());
}


void PrettyPrinter::VisitForOfStatement(ForOfStatement* node) {
  PrintLabels(node->labels());
  Print("for (");
  Visit(node->each());
  Print(" of ");
  Visit(node->iterable());
  Print(") ");
  Visit(node->body());
}


void PrettyPrinter::VisitTryCatchStatement(TryCatchStatement* node) {
  Print("try ");
  Visit(node->try_block());
  Print(" catch (");
  const bool quote = false;
  PrintLiteral(node->variable()->name(), quote);
  Print(") ");
  Visit(node->catch_block());
}


void PrettyPrinter::VisitTryFinallyStatement(TryFinallyStatement* node) {
  Print("try ");
  Visit(node->try_block());
  Print(" finally ");
  Visit(node->finally_block());
}


void PrettyPrinter::VisitDebuggerStatement(DebuggerStatement* node) {
  Print("debugger ");
}


void PrettyPrinter::VisitFunctionLiteral(FunctionLiteral* node) {
  Print("(");
  PrintFunctionLiteral(node);
  Print(")");
}


void PrettyPrinter::VisitClassLiteral(ClassLiteral* node) {
  Print("(class ");
  PrintLiteral(node->name(), false);
  if (node->extends()) {
    Print(" extends ");
    Visit(node->extends());
  }
  Print(" { ");
  for (int i = 0; i < node->properties()->length(); i++) {
    PrintObjectLiteralProperty(node->properties()->at(i));
  }
  Print(" })");
}


void PrettyPrinter::VisitNativeFunctionLiteral(NativeFunctionLiteral* node) {
  Print("(");
  PrintLiteral(node->name(), false);
  Print(")");
}


void PrettyPrinter::VisitConditional(Conditional* node) {
  Visit(node->condition());
  Print(" ? ");
  Visit(node->then_expression());
  Print(" : ");
  Visit(node->else_expression());
}


void PrettyPrinter::VisitLiteral(Literal* node) {
  PrintLiteral(node->value(), true);
}


void PrettyPrinter::VisitRegExpLiteral(RegExpLiteral* node) {
  Print(" RegExp(");
  PrintLiteral(node->pattern(), false);
  Print(",");
  PrintLiteral(node->flags(), false);
  Print(") ");
}


void PrettyPrinter::VisitObjectLiteral(ObjectLiteral* node) {
  Print("{ ");
  for (int i = 0; i < node->properties()->length(); i++) {
    if (i != 0) Print(",");
    PrintObjectLiteralProperty(node->properties()->at(i));
  }
  Print(" }");
}


void PrettyPrinter::PrintObjectLiteralProperty(
    ObjectLiteralProperty* property) {
  // TODO(arv): Better printing of methods etc.
  Print(" ");
  Visit(property->key());
  Print(": ");
  Visit(property->value());
}


void PrettyPrinter::VisitArrayLiteral(ArrayLiteral* node) {
  Print("[ ");
  for (int i = 0; i < node->values()->length(); i++) {
    if (i != 0) Print(",");
    Visit(node->values()->at(i));
  }
  Print(" ]");
}


void PrettyPrinter::VisitVariableProxy(VariableProxy* node) {
  PrintLiteral(node->name(), false);
}


void PrettyPrinter::VisitAssignment(Assignment* node) {
  Visit(node->target());
  Print(" %s ", Token::String(node->op()));
  Visit(node->value());
}


void PrettyPrinter::VisitYield(Yield* node) {
  Print("yield ");
  Visit(node->expression());
}


void PrettyPrinter::VisitThrow(Throw* node) {
  Print("throw ");
  Visit(node->exception());
}


void PrettyPrinter::VisitProperty(Property* node) {
  Expression* key = node->key();
  Literal* literal = key->AsLiteral();
  if (literal != NULL && literal->value()->IsInternalizedString()) {
    Print("(");
    Visit(node->obj());
    Print(").");
    PrintLiteral(literal->value(), false);
  } else {
    Visit(node->obj());
    Print("[");
    Visit(key);
    Print("]");
  }
}


void PrettyPrinter::VisitCall(Call* node) {
  Visit(node->expression());
  PrintArguments(node->arguments());
}


void PrettyPrinter::VisitCallNew(CallNew* node) {
  Print("new (");
  Visit(node->expression());
  Print(")");
  PrintArguments(node->arguments());
}


void PrettyPrinter::VisitCallRuntime(CallRuntime* node) {
  Print("%%");
  PrintLiteral(node->name(), false);
  PrintArguments(node->arguments());
}


void PrettyPrinter::VisitUnaryOperation(UnaryOperation* node) {
  Token::Value op = node->op();
  bool needsSpace =
      op == Token::DELETE || op == Token::TYPEOF || op == Token::VOID;
  Print("(%s%s", Token::String(op), needsSpace ? " " : "");
  Visit(node->expression());
  Print(")");
}


void PrettyPrinter::VisitCountOperation(CountOperation* node) {
  Print("(");
  if (node->is_prefix()) Print("%s", Token::String(node->op()));
  Visit(node->expression());
  if (node->is_postfix()) Print("%s", Token::String(node->op()));
  Print(")");
}


void PrettyPrinter::VisitBinaryOperation(BinaryOperation* node) {
  Print("(");
  Visit(node->left());
  Print(" %s ", Token::String(node->op()));
  Visit(node->right());
  Print(")");
}


void PrettyPrinter::VisitCompareOperation(CompareOperation* node) {
  Print("(");
  Visit(node->left());
  Print(" %s ", Token::String(node->op()));
  Visit(node->right());
  Print(")");
}


void PrettyPrinter::VisitThisFunction(ThisFunction* node) {
  Print("<this-function>");
}


void PrettyPrinter::VisitSuperReference(SuperReference* node) {
  Print("<super-reference>");
}


const char* PrettyPrinter::Print(AstNode* node) {
  Init();
  Visit(node);
  return output_;
}


const char* PrettyPrinter::PrintExpression(FunctionLiteral* program) {
  Init();
  ExpressionStatement* statement =
    program->body()->at(0)->AsExpressionStatement();
  Visit(statement->expression());
  return output_;
}


const char* PrettyPrinter::PrintProgram(FunctionLiteral* program) {
  Init();
  PrintStatements(program->body());
  Print("\n");
  return output_;
}


void PrettyPrinter::PrintOut(Isolate* isolate, Zone* zone, AstNode* node) {
  PrettyPrinter printer(isolate, zone);
  PrintF("%s", printer.Print(node));
}


void PrettyPrinter::Init() {
  if (size_ == 0) {
    DCHECK(output_ == NULL);
    const int initial_size = 256;
    output_ = NewArray<char>(initial_size);
    size_ = initial_size;
  }
  output_[0] = '\0';
  pos_ = 0;
}


void PrettyPrinter::Print(const char* format, ...) {
  for (;;) {
    va_list arguments;
    va_start(arguments, format);
    int n = VSNPrintF(Vector<char>(output_, size_) + pos_,
                      format,
                      arguments);
    va_end(arguments);

    if (n >= 0) {
      // there was enough space - we are done
      pos_ += n;
      return;
    } else {
      // there was not enough space - allocate more and try again
      const int slack = 32;
      int new_size = size_ + (size_ >> 1) + slack;
      char* new_output = NewArray<char>(new_size);
      MemCopy(new_output, output_, pos_);
      DeleteArray(output_);
      output_ = new_output;
      size_ = new_size;
    }
  }
}


void PrettyPrinter::PrintStatements(ZoneList<Statement*>* statements) {
  if (statements == NULL) return;
  for (int i = 0; i < statements->length(); i++) {
    if (i != 0) Print(" ");
    Visit(statements->at(i));
  }
}


void PrettyPrinter::PrintLabels(ZoneList<const AstRawString*>* labels) {
  if (labels != NULL) {
    for (int i = 0; i < labels->length(); i++) {
      PrintLiteral(labels->at(i), false);
      Print(": ");
    }
  }
}


void PrettyPrinter::PrintArguments(ZoneList<Expression*>* arguments) {
  Print("(");
  for (int i = 0; i < arguments->length(); i++) {
    if (i != 0) Print(", ");
    Visit(arguments->at(i));
  }
  Print(")");
}


void PrettyPrinter::PrintLiteral(Handle<Object> value, bool quote) {
  Object* object = *value;
  if (object->IsString()) {
    String* string = String::cast(object);
    if (quote) Print("\"");
    for (int i = 0; i < string->length(); i++) {
      Print("%c", string->Get(i));
    }
    if (quote) Print("\"");
  } else if (object->IsNull()) {
    Print("null");
  } else if (object->IsTrue()) {
    Print("true");
  } else if (object->IsFalse()) {
    Print("false");
  } else if (object->IsUndefined()) {
    Print("undefined");
  } else if (object->IsNumber()) {
    Print("%g", object->Number());
  } else if (object->IsJSObject()) {
    // regular expression
    if (object->IsJSFunction()) {
      Print("JS-Function");
    } else if (object->IsJSArray()) {
      Print("JS-array[%u]", JSArray::cast(object)->length());
    } else if (object->IsJSObject()) {
      Print("JS-Object");
    } else {
      Print("?UNKNOWN?");
    }
  } else if (object->IsFixedArray()) {
    Print("FixedArray");
  } else {
    Print("<unknown literal %p>", object);
  }
}


void PrettyPrinter::PrintLiteral(const AstRawString* value, bool quote) {
  PrintLiteral(value->string(), quote);
}


void PrettyPrinter::PrintParameters(Scope* scope) {
  Print("(");
  for (int i = 0; i < scope->num_parameters(); i++) {
    if (i  > 0) Print(", ");
    PrintLiteral(scope->parameter(i)->name(), false);
  }
  Print(")");
}


void PrettyPrinter::PrintDeclarations(ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    if (i > 0) Print(" ");
    Visit(declarations->at(i));
  }
}


void PrettyPrinter::PrintFunctionLiteral(FunctionLiteral* function) {
  Print("function ");
  PrintLiteral(function->name(), false);
  PrintParameters(function->scope());
  Print(" { ");
  PrintDeclarations(function->scope()->declarations());
  PrintStatements(function->body());
  Print(" }");
}


//-----------------------------------------------------------------------------

class IndentedScope BASE_EMBEDDED {
 public:
  IndentedScope(AstPrinter* printer, const char* txt)
      : ast_printer_(printer) {
    ast_printer_->PrintIndented(txt);
    ast_printer_->Print("\n");
    ast_printer_->inc_indent();
  }

  virtual ~IndentedScope() {
    ast_printer_->dec_indent();
  }

 private:
  AstPrinter* ast_printer_;
};


//-----------------------------------------------------------------------------


AstPrinter::AstPrinter(Isolate* isolate, Zone* zone)
    : PrettyPrinter(isolate, zone), indent_(0) {}


AstPrinter::~AstPrinter() {
  DCHECK(indent_ == 0);
}


void AstPrinter::PrintIndented(const char* txt) {
  for (int i = 0; i < indent_; i++) {
    Print(". ");
  }
  Print(txt);
}


void AstPrinter::PrintLiteralIndented(const char* info,
                                      Handle<Object> value,
                                      bool quote) {
  PrintIndented(info);
  Print(" ");
  PrintLiteral(value, quote);
  Print("\n");
}


void AstPrinter::PrintLiteralWithModeIndented(const char* info,
                                              Variable* var,
                                              Handle<Object> value) {
  if (var == NULL) {
    PrintLiteralIndented(info, value, true);
  } else {
    EmbeddedVector<char, 256> buf;
    int pos = SNPrintF(buf, "%s (mode = %s", info,
                       Variable::Mode2String(var->mode()));
    SNPrintF(buf + pos, ")");
    PrintLiteralIndented(buf.start(), value, true);
  }
}


void AstPrinter::PrintLabelsIndented(ZoneList<const AstRawString*>* labels) {
  if (labels == NULL || labels->length() == 0) return;
  PrintIndented("LABELS ");
  PrintLabels(labels);
  Print("\n");
}


void AstPrinter::PrintIndentedVisit(const char* s, AstNode* node) {
  IndentedScope indent(this, s);
  Visit(node);
}


const char* AstPrinter::PrintProgram(FunctionLiteral* program) {
  Init();
  { IndentedScope indent(this, "FUNC");
    PrintLiteralIndented("NAME", program->name(), true);
    PrintLiteralIndented("INFERRED NAME", program->inferred_name(), true);
    PrintParameters(program->scope());
    PrintDeclarations(program->scope()->declarations());
    PrintStatements(program->body());
  }
  return Output();
}


void AstPrinter::PrintDeclarations(ZoneList<Declaration*>* declarations) {
  if (declarations->length() > 0) {
    IndentedScope indent(this, "DECLS");
    for (int i = 0; i < declarations->length(); i++) {
      Visit(declarations->at(i));
    }
  }
}


void AstPrinter::PrintParameters(Scope* scope) {
  if (scope->num_parameters() > 0) {
    IndentedScope indent(this, "PARAMS");
    for (int i = 0; i < scope->num_parameters(); i++) {
      PrintLiteralWithModeIndented("VAR", scope->parameter(i),
                                   scope->parameter(i)->name());
    }
  }
}


void AstPrinter::PrintStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    Visit(statements->at(i));
  }
}


void AstPrinter::PrintArguments(ZoneList<Expression*>* arguments) {
  for (int i = 0; i < arguments->length(); i++) {
    Visit(arguments->at(i));
  }
}


void AstPrinter::VisitBlock(Block* node) {
  const char* block_txt = node->is_initializer_block() ? "BLOCK INIT" : "BLOCK";
  IndentedScope indent(this, block_txt);
  PrintStatements(node->statements());
}


// TODO(svenpanne) Start with IndentedScope.
void AstPrinter::VisitVariableDeclaration(VariableDeclaration* node) {
  PrintLiteralWithModeIndented(Variable::Mode2String(node->mode()),
                               node->proxy()->var(),
                               node->proxy()->name());
}


// TODO(svenpanne) Start with IndentedScope.
void AstPrinter::VisitFunctionDeclaration(FunctionDeclaration* node) {
  PrintIndented("FUNCTION ");
  PrintLiteral(node->proxy()->name(), true);
  Print(" = function ");
  PrintLiteral(node->fun()->name(), false);
  Print("\n");
}


void AstPrinter::VisitModuleDeclaration(ModuleDeclaration* node) {
  IndentedScope indent(this, "MODULE");
  PrintLiteralIndented("NAME", node->proxy()->name(), true);
  Visit(node->module());
}


void AstPrinter::VisitImportDeclaration(ImportDeclaration* node) {
  IndentedScope indent(this, "IMPORT");
  PrintLiteralIndented("NAME", node->proxy()->name(), true);
  PrintLiteralIndented("FROM", node->module_specifier()->string(), true);
}


void AstPrinter::VisitExportDeclaration(ExportDeclaration* node) {
  IndentedScope indent(this, "EXPORT ");
  PrintLiteral(node->proxy()->name(), true);
}


void AstPrinter::VisitModuleLiteral(ModuleLiteral* node) {
  IndentedScope indent(this, "MODULE LITERAL");
  VisitBlock(node->body());
}


void AstPrinter::VisitModulePath(ModulePath* node) {
  IndentedScope indent(this, "MODULE PATH");
  PrintIndentedVisit("MODULE PATH PARENT", node->module());
  PrintLiteralIndented("NAME", node->name(), true);
}


void AstPrinter::VisitModuleUrl(ModuleUrl* node) {
  PrintLiteralIndented("URL", node->url(), true);
}


void AstPrinter::VisitModuleStatement(ModuleStatement* node) {
  IndentedScope indent(this, "MODULE STATEMENT");
  PrintStatements(node->body()->statements());
}


void AstPrinter::VisitExpressionStatement(ExpressionStatement* node) {
  IndentedScope indent(this, "EXPRESSION STATEMENT");
  Visit(node->expression());
}


void AstPrinter::VisitEmptyStatement(EmptyStatement* node) {
  IndentedScope indent(this, "EMPTY");
}


void AstPrinter::VisitIfStatement(IfStatement* node) {
  IndentedScope indent(this, "IF");
  PrintIndentedVisit("CONDITION", node->condition());
  PrintIndentedVisit("THEN", node->then_statement());
  if (node->HasElseStatement()) {
    PrintIndentedVisit("ELSE", node->else_statement());
  }
}


void AstPrinter::VisitContinueStatement(ContinueStatement* node) {
  IndentedScope indent(this, "CONTINUE");
  PrintLabelsIndented(node->target()->labels());
}


void AstPrinter::VisitBreakStatement(BreakStatement* node) {
  IndentedScope indent(this, "BREAK");
  PrintLabelsIndented(node->target()->labels());
}


void AstPrinter::VisitReturnStatement(ReturnStatement* node) {
  IndentedScope indent(this, "RETURN");
  Visit(node->expression());
}


void AstPrinter::VisitWithStatement(WithStatement* node) {
  IndentedScope indent(this, "WITH");
  PrintIndentedVisit("OBJECT", node->expression());
  PrintIndentedVisit("BODY", node->statement());
}


void AstPrinter::VisitSwitchStatement(SwitchStatement* node) {
  IndentedScope indent(this, "SWITCH");
  PrintLabelsIndented(node->labels());
  PrintIndentedVisit("TAG", node->tag());
  for (int i = 0; i < node->cases()->length(); i++) {
    Visit(node->cases()->at(i));
  }
}


void AstPrinter::VisitCaseClause(CaseClause* clause) {
  if (clause->is_default()) {
    IndentedScope indent(this, "DEFAULT");
    PrintStatements(clause->statements());
  } else {
    IndentedScope indent(this, "CASE");
    Visit(clause->label());
    PrintStatements(clause->statements());
  }
}


void AstPrinter::VisitDoWhileStatement(DoWhileStatement* node) {
  IndentedScope indent(this, "DO");
  PrintLabelsIndented(node->labels());
  PrintIndentedVisit("BODY", node->body());
  PrintIndentedVisit("COND", node->cond());
}


void AstPrinter::VisitWhileStatement(WhileStatement* node) {
  IndentedScope indent(this, "WHILE");
  PrintLabelsIndented(node->labels());
  PrintIndentedVisit("COND", node->cond());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitForStatement(ForStatement* node) {
  IndentedScope indent(this, "FOR");
  PrintLabelsIndented(node->labels());
  if (node->init()) PrintIndentedVisit("INIT", node->init());
  if (node->cond()) PrintIndentedVisit("COND", node->cond());
  PrintIndentedVisit("BODY", node->body());
  if (node->next()) PrintIndentedVisit("NEXT", node->next());
}


void AstPrinter::VisitForInStatement(ForInStatement* node) {
  IndentedScope indent(this, "FOR IN");
  PrintIndentedVisit("FOR", node->each());
  PrintIndentedVisit("IN", node->enumerable());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitForOfStatement(ForOfStatement* node) {
  IndentedScope indent(this, "FOR OF");
  PrintIndentedVisit("FOR", node->each());
  PrintIndentedVisit("OF", node->iterable());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitTryCatchStatement(TryCatchStatement* node) {
  IndentedScope indent(this, "TRY CATCH");
  PrintIndentedVisit("TRY", node->try_block());
  PrintLiteralWithModeIndented("CATCHVAR",
                               node->variable(),
                               node->variable()->name());
  PrintIndentedVisit("CATCH", node->catch_block());
}


void AstPrinter::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IndentedScope indent(this, "TRY FINALLY");
  PrintIndentedVisit("TRY", node->try_block());
  PrintIndentedVisit("FINALLY", node->finally_block());
}


void AstPrinter::VisitDebuggerStatement(DebuggerStatement* node) {
  IndentedScope indent(this, "DEBUGGER");
}


void AstPrinter::VisitFunctionLiteral(FunctionLiteral* node) {
  IndentedScope indent(this, "FUNC LITERAL");
  PrintLiteralIndented("NAME", node->name(), false);
  PrintLiteralIndented("INFERRED NAME", node->inferred_name(), false);
  PrintParameters(node->scope());
  // We don't want to see the function literal in this case: it
  // will be printed via PrintProgram when the code for it is
  // generated.
  // PrintStatements(node->body());
}


void AstPrinter::VisitClassLiteral(ClassLiteral* node) {
  IndentedScope indent(this, "CLASS LITERAL");
  PrintLiteralIndented("NAME", node->name(), false);
}


void AstPrinter::VisitNativeFunctionLiteral(NativeFunctionLiteral* node) {
  IndentedScope indent(this, "NATIVE FUNC LITERAL");
  PrintLiteralIndented("NAME", node->name(), false);
}


void AstPrinter::VisitConditional(Conditional* node) {
  IndentedScope indent(this, "CONDITIONAL");
  PrintIndentedVisit("CONDITION", node->condition());
  PrintIndentedVisit("THEN", node->then_expression());
  PrintIndentedVisit("ELSE", node->else_expression());
}


// TODO(svenpanne) Start with IndentedScope.
void AstPrinter::VisitLiteral(Literal* node) {
  PrintLiteralIndented("LITERAL", node->value(), true);
}


void AstPrinter::VisitRegExpLiteral(RegExpLiteral* node) {
  IndentedScope indent(this, "REGEXP LITERAL");
  PrintLiteralIndented("PATTERN", node->pattern(), false);
  PrintLiteralIndented("FLAGS", node->flags(), false);
}


void AstPrinter::VisitObjectLiteral(ObjectLiteral* node) {
  IndentedScope indent(this, "OBJ LITERAL");
  for (int i = 0; i < node->properties()->length(); i++) {
    const char* prop_kind = NULL;
    switch (node->properties()->at(i)->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        prop_kind = "PROPERTY - CONSTANT";
        break;
      case ObjectLiteral::Property::COMPUTED:
        prop_kind = "PROPERTY - COMPUTED";
        break;
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        prop_kind = "PROPERTY - MATERIALIZED_LITERAL";
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        prop_kind = "PROPERTY - PROTOTYPE";
        break;
      case ObjectLiteral::Property::GETTER:
        prop_kind = "PROPERTY - GETTER";
        break;
      case ObjectLiteral::Property::SETTER:
        prop_kind = "PROPERTY - SETTER";
        break;
      default:
        UNREACHABLE();
    }
    IndentedScope prop(this, prop_kind);
    PrintIndentedVisit("KEY", node->properties()->at(i)->key());
    PrintIndentedVisit("VALUE", node->properties()->at(i)->value());
  }
}


void AstPrinter::VisitArrayLiteral(ArrayLiteral* node) {
  IndentedScope indent(this, "ARRAY LITERAL");
  if (node->values()->length() > 0) {
    IndentedScope indent(this, "VALUES");
    for (int i = 0; i < node->values()->length(); i++) {
      Visit(node->values()->at(i));
    }
  }
}


// TODO(svenpanne) Start with IndentedScope.
void AstPrinter::VisitVariableProxy(VariableProxy* node) {
  Variable* var = node->var();
  EmbeddedVector<char, 128> buf;
  int pos = SNPrintF(buf, "VAR PROXY");
  switch (var->location()) {
    case Variable::UNALLOCATED:
      break;
    case Variable::PARAMETER:
      SNPrintF(buf + pos, " parameter[%d]", var->index());
      break;
    case Variable::LOCAL:
      SNPrintF(buf + pos, " local[%d]", var->index());
      break;
    case Variable::CONTEXT:
      SNPrintF(buf + pos, " context[%d]", var->index());
      break;
    case Variable::LOOKUP:
      SNPrintF(buf + pos, " lookup");
      break;
  }
  PrintLiteralWithModeIndented(buf.start(), var, node->name());
}


void AstPrinter::VisitAssignment(Assignment* node) {
  IndentedScope indent(this, Token::Name(node->op()));
  Visit(node->target());
  Visit(node->value());
}


void AstPrinter::VisitYield(Yield* node) {
  IndentedScope indent(this, "YIELD");
  Visit(node->expression());
}


void AstPrinter::VisitThrow(Throw* node) {
  IndentedScope indent(this, "THROW");
  Visit(node->exception());
}


void AstPrinter::VisitProperty(Property* node) {
  IndentedScope indent(this, "PROPERTY");
  Visit(node->obj());
  Literal* literal = node->key()->AsLiteral();
  if (literal != NULL && literal->value()->IsInternalizedString()) {
    PrintLiteralIndented("NAME", literal->value(), false);
  } else {
    PrintIndentedVisit("KEY", node->key());
  }
}


void AstPrinter::VisitCall(Call* node) {
  IndentedScope indent(this, "CALL");
  Visit(node->expression());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitCallNew(CallNew* node) {
  IndentedScope indent(this, "CALL NEW");
  Visit(node->expression());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitCallRuntime(CallRuntime* node) {
  IndentedScope indent(this, "CALL RUNTIME");
  PrintLiteralIndented("NAME", node->name(), false);
  PrintArguments(node->arguments());
}


void AstPrinter::VisitUnaryOperation(UnaryOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()));
  Visit(node->expression());
}


void AstPrinter::VisitCountOperation(CountOperation* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "%s %s", (node->is_prefix() ? "PRE" : "POST"),
           Token::Name(node->op()));
  IndentedScope indent(this, buf.start());
  Visit(node->expression());
}


void AstPrinter::VisitBinaryOperation(BinaryOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()));
  Visit(node->left());
  Visit(node->right());
}


void AstPrinter::VisitCompareOperation(CompareOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()));
  Visit(node->left());
  Visit(node->right());
}


void AstPrinter::VisitThisFunction(ThisFunction* node) {
  IndentedScope indent(this, "THIS-FUNCTION");
}


void AstPrinter::VisitSuperReference(SuperReference* node) {
  IndentedScope indent(this, "SUPER-REFERENCE");
}

#endif  // DEBUG

} }  // namespace v8::internal
