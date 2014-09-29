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

#ifdef DEBUG

PrettyPrinter::PrettyPrinter(Zone* zone) {
  output_ = NULL;
  size_ = 0;
  pos_ = 0;
  InitializeAstVisitor(zone);
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
  Visit(node->module());
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


void PrettyPrinter::VisitModuleVariable(ModuleVariable* node) {
  Visit(node->proxy());
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
  PrintLiteral(node->proxy()->name(), false);
  Print(" ");
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
    ObjectLiteral::Property* property = node->properties()->at(i);
    Print(" ");
    Visit(property->key());
    Print(": ");
    Visit(property->value());
  }
  Print(" }");
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


void PrettyPrinter::PrintOut(Zone* zone, AstNode* node) {
  PrettyPrinter printer(zone);
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


AstPrinter::AstPrinter(Zone* zone) : PrettyPrinter(zone), indent_(0) {
}


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
  Visit(node->module());
}


void AstPrinter::VisitExportDeclaration(ExportDeclaration* node) {
  IndentedScope indent(this, "EXPORT ");
  PrintLiteral(node->proxy()->name(), true);
}


void AstPrinter::VisitModuleLiteral(ModuleLiteral* node) {
  IndentedScope indent(this, "MODULE LITERAL");
  VisitBlock(node->body());
}


void AstPrinter::VisitModuleVariable(ModuleVariable* node) {
  IndentedScope indent(this, "MODULE VARIABLE");
  Visit(node->proxy());
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
  PrintLiteralIndented("NAME", node->proxy()->name(), true);
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

#endif  // DEBUG

} }  // namespace v8::internal
