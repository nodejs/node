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

#include <stdarg.h>

#include "v8.h"

#include "prettyprinter.h"
#include "scopes.h"
#include "platform.h"

namespace v8 {
namespace internal {

#ifdef DEBUG

PrettyPrinter::PrettyPrinter() {
  output_ = NULL;
  size_ = 0;
  pos_ = 0;
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


void PrettyPrinter::VisitDeclaration(Declaration* node) {
  Print("var ");
  PrintLiteral(node->proxy()->name(), false);
  if (node->fun() != NULL) {
    Print(" = ");
    PrintFunctionLiteral(node->fun());
  }
  Print(";");
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
  ZoneStringList* labels = node->target()->labels();
  if (labels != NULL) {
    Print(" ");
    ASSERT(labels->length() > 0);  // guaranteed to have at least one entry
    PrintLiteral(labels->at(0), false);  // any label from the list is fine
  }
  Print(";");
}


void PrettyPrinter::VisitBreakStatement(BreakStatement* node) {
  Print("break");
  ZoneStringList* labels = node->target()->labels();
  if (labels != NULL) {
    Print(" ");
    ASSERT(labels->length() > 0);  // guaranteed to have at least one entry
    PrintLiteral(labels->at(0), false);  // any label from the list is fine
  }
  Print(";");
}


void PrettyPrinter::VisitReturnStatement(ReturnStatement* node) {
  Print("return ");
  Visit(node->expression());
  Print(";");
}


void PrettyPrinter::VisitWithEnterStatement(WithEnterStatement* node) {
  Print("<enter with> (");
  Visit(node->expression());
  Print(") ");
}


void PrettyPrinter::VisitWithExitStatement(WithExitStatement* node) {
  Print("<exit with>");
}


void PrettyPrinter::VisitSwitchStatement(SwitchStatement* node) {
  PrintLabels(node->labels());
  Print("switch (");
  Visit(node->tag());
  Print(") { ");
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = 0; i < cases->length(); i++)
    PrintCaseClause(cases->at(i));
  Print("}");
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


void PrettyPrinter::VisitTryCatchStatement(TryCatchStatement* node) {
  Print("try ");
  Visit(node->try_block());
  Print(" catch (");
  Visit(node->catch_var());
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


void PrettyPrinter::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  Print("(");
  PrintLiteral(node->boilerplate(), true);
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
  PrintLiteral(node->handle(), true);
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


void PrettyPrinter::VisitCatchExtensionObject(CatchExtensionObject* node) {
  Print("{ ");
  Visit(node->key());
  Print(": ");
  Visit(node->value());
  Print(" }");
}


void PrettyPrinter::VisitSlot(Slot* node) {
  switch (node->type()) {
    case Slot::PARAMETER:
      Print("parameter[%d]", node->index());
      break;
    case Slot::LOCAL:
      Print("frame[%d]", node->index());
      break;
    case Slot::CONTEXT:
      Print(".context[%d]", node->index());
      break;
    case Slot::LOOKUP:
      Print(".context[");
      PrintLiteral(node->var()->name(), false);
      Print("]");
      break;
    default:
      UNREACHABLE();
  }
}


void PrettyPrinter::VisitVariableProxy(VariableProxy* node) {
  PrintLiteral(node->name(), false);
}


void PrettyPrinter::VisitAssignment(Assignment* node) {
  Visit(node->target());
  Print(" %s ", Token::String(node->op()));
  Visit(node->value());
}


void PrettyPrinter::VisitThrow(Throw* node) {
  Print("throw ");
  Visit(node->exception());
}


void PrettyPrinter::VisitProperty(Property* node) {
  Expression* key = node->key();
  Literal* literal = key->AsLiteral();
  if (literal != NULL && literal->handle()->IsSymbol()) {
    Print("(");
    Visit(node->obj());
    Print(").");
    PrintLiteral(literal->handle(), false);
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
  Print("(%s", Token::String(node->op()));
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
  Print("%s", Token::String(node->op()));
  Visit(node->right());
  Print(")");
}


void PrettyPrinter::VisitCompareOperation(CompareOperation* node) {
  Print("(");
  Visit(node->left());
  Print("%s", Token::String(node->op()));
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


void PrettyPrinter::PrintOut(AstNode* node) {
  PrettyPrinter printer;
  PrintF("%s", printer.Print(node));
}


void PrettyPrinter::Init() {
  if (size_ == 0) {
    ASSERT(output_ == NULL);
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
    int n = OS::VSNPrintF(Vector<char>(output_, size_) + pos_,
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
      memcpy(new_output, output_, pos_);
      DeleteArray(output_);
      output_ = new_output;
      size_ = new_size;
    }
  }
}


void PrettyPrinter::PrintStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    if (i != 0) Print(" ");
    Visit(statements->at(i));
  }
}


void PrettyPrinter::PrintLabels(ZoneStringList* labels) {
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
  } else if (object == Heap::null_value()) {
    Print("null");
  } else if (object == Heap::true_value()) {
    Print("true");
  } else if (object == Heap::false_value()) {
    Print("false");
  } else if (object == Heap::undefined_value()) {
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


void PrettyPrinter::PrintCaseClause(CaseClause* clause) {
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


//-----------------------------------------------------------------------------

class IndentedScope BASE_EMBEDDED {
 public:
  IndentedScope() {
    ast_printer_->inc_indent();
  }

  explicit IndentedScope(const char* txt, AstNode* node = NULL) {
    ast_printer_->PrintIndented(txt);
    if (node != NULL && node->AsExpression() != NULL) {
      Expression* expr = node->AsExpression();
      bool printed_first = false;
      if ((expr->type() != NULL) && (expr->type()->IsKnown())) {
        ast_printer_->Print(" (type = ");
        ast_printer_->Print(StaticType::Type2String(expr->type()));
        printed_first = true;
      }
      if (expr->num() != Expression::kNoLabel) {
        ast_printer_->Print(printed_first ? ", num = " : " (num = ");
        ast_printer_->Print("%d", expr->num());
        printed_first = true;
      }
      if (printed_first) ast_printer_->Print(")");
    }
    ast_printer_->Print("\n");
    ast_printer_->inc_indent();
  }

  virtual ~IndentedScope() {
    ast_printer_->dec_indent();
  }

  static void SetAstPrinter(AstPrinter* a) { ast_printer_ = a; }

 private:
  static AstPrinter* ast_printer_;
};


AstPrinter* IndentedScope::ast_printer_ = NULL;


//-----------------------------------------------------------------------------

int AstPrinter::indent_ = 0;


AstPrinter::AstPrinter() {
  ASSERT(indent_ == 0);
  IndentedScope::SetAstPrinter(this);
}


AstPrinter::~AstPrinter() {
  ASSERT(indent_ == 0);
  IndentedScope::SetAstPrinter(NULL);
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
                                              Handle<Object> value,
                                              StaticType* type,
                                              int num) {
  if (var == NULL) {
    PrintLiteralIndented(info, value, true);
  } else {
    EmbeddedVector<char, 256> buf;
    int pos = OS::SNPrintF(buf, "%s (mode = %s", info,
                           Variable::Mode2String(var->mode()));
    if (type->IsKnown()) {
      pos += OS::SNPrintF(buf + pos, ", type = %s",
                          StaticType::Type2String(type));
    }
    if (num != Expression::kNoLabel) {
      pos += OS::SNPrintF(buf + pos, ", num = %d", num);
    }
    OS::SNPrintF(buf + pos, ")");
    PrintLiteralIndented(buf.start(), value, true);
  }
}


void AstPrinter::PrintLabelsIndented(const char* info, ZoneStringList* labels) {
  if (labels != NULL && labels->length() > 0) {
    if (info == NULL) {
      PrintIndented("LABELS ");
    } else {
      PrintIndented(info);
      Print(" ");
    }
    PrintLabels(labels);
  } else if (info != NULL) {
    PrintIndented(info);
  }
  Print("\n");
}


void AstPrinter::PrintIndentedVisit(const char* s, AstNode* node) {
  IndentedScope indent(s, node);
  Visit(node);
}


const char* AstPrinter::PrintProgram(FunctionLiteral* program) {
  Init();
  { IndentedScope indent("FUNC");
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
    IndentedScope indent("DECLS");
    for (int i = 0; i < declarations->length(); i++) {
      Visit(declarations->at(i));
    }
  }
}


void AstPrinter::PrintParameters(Scope* scope) {
  if (scope->num_parameters() > 0) {
    IndentedScope indent("PARAMS");
    for (int i = 0; i < scope->num_parameters(); i++) {
      PrintLiteralWithModeIndented("VAR", scope->parameter(i),
                                   scope->parameter(i)->name(),
                                   scope->parameter(i)->type(),
                                   Expression::kNoLabel);
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


void AstPrinter::PrintCaseClause(CaseClause* clause) {
  if (clause->is_default()) {
    IndentedScope indent("DEFAULT");
    PrintStatements(clause->statements());
  } else {
    IndentedScope indent("CASE");
    Visit(clause->label());
    PrintStatements(clause->statements());
  }
}


void AstPrinter::VisitBlock(Block* node) {
  const char* block_txt = node->is_initializer_block() ? "BLOCK INIT" : "BLOCK";
  IndentedScope indent(block_txt);
  PrintStatements(node->statements());
}


void AstPrinter::VisitDeclaration(Declaration* node) {
  if (node->fun() == NULL) {
    // var or const declarations
    PrintLiteralWithModeIndented(Variable::Mode2String(node->mode()),
                                 node->proxy()->AsVariable(),
                                 node->proxy()->name(),
                                 node->proxy()->AsVariable()->type(),
                                 Expression::kNoLabel);
  } else {
    // function declarations
    PrintIndented("FUNCTION ");
    PrintLiteral(node->proxy()->name(), true);
    Print(" = function ");
    PrintLiteral(node->fun()->name(), false);
    Print("\n");
  }
}


void AstPrinter::VisitExpressionStatement(ExpressionStatement* node) {
  Visit(node->expression());
}


void AstPrinter::VisitEmptyStatement(EmptyStatement* node) {
  PrintIndented("EMPTY\n");
}


void AstPrinter::VisitIfStatement(IfStatement* node) {
  PrintIndentedVisit("IF", node->condition());
  PrintIndentedVisit("THEN", node->then_statement());
  if (node->HasElseStatement()) {
    PrintIndentedVisit("ELSE", node->else_statement());
  }
}


void AstPrinter::VisitContinueStatement(ContinueStatement* node) {
  PrintLabelsIndented("CONTINUE", node->target()->labels());
}


void AstPrinter::VisitBreakStatement(BreakStatement* node) {
  PrintLabelsIndented("BREAK", node->target()->labels());
}


void AstPrinter::VisitReturnStatement(ReturnStatement* node) {
  PrintIndentedVisit("RETURN", node->expression());
}


void AstPrinter::VisitWithEnterStatement(WithEnterStatement* node) {
  PrintIndentedVisit("WITH ENTER", node->expression());
}


void AstPrinter::VisitWithExitStatement(WithExitStatement* node) {
  PrintIndented("WITH EXIT\n");
}


void AstPrinter::VisitSwitchStatement(SwitchStatement* node) {
  IndentedScope indent("SWITCH");
  PrintLabelsIndented(NULL, node->labels());
  PrintIndentedVisit("TAG", node->tag());
  for (int i = 0; i < node->cases()->length(); i++) {
    PrintCaseClause(node->cases()->at(i));
  }
}


void AstPrinter::VisitDoWhileStatement(DoWhileStatement* node) {
  IndentedScope indent("DO");
  PrintLabelsIndented(NULL, node->labels());
  PrintIndentedVisit("BODY", node->body());
  PrintIndentedVisit("COND", node->cond());
}


void AstPrinter::VisitWhileStatement(WhileStatement* node) {
  IndentedScope indent("WHILE");
  PrintLabelsIndented(NULL, node->labels());
  PrintIndentedVisit("COND", node->cond());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitForStatement(ForStatement* node) {
  IndentedScope indent("FOR");
  PrintLabelsIndented(NULL, node->labels());
  if (node->init()) PrintIndentedVisit("INIT", node->init());
  if (node->cond()) PrintIndentedVisit("COND", node->cond());
  PrintIndentedVisit("BODY", node->body());
  if (node->next()) PrintIndentedVisit("NEXT", node->next());
}


void AstPrinter::VisitForInStatement(ForInStatement* node) {
  IndentedScope indent("FOR IN");
  PrintIndentedVisit("FOR", node->each());
  PrintIndentedVisit("IN", node->enumerable());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitTryCatchStatement(TryCatchStatement* node) {
  IndentedScope indent("TRY CATCH");
  PrintIndentedVisit("TRY", node->try_block());
  PrintIndentedVisit("CATCHVAR", node->catch_var());
  PrintIndentedVisit("CATCH", node->catch_block());
}


void AstPrinter::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IndentedScope indent("TRY FINALLY");
  PrintIndentedVisit("TRY", node->try_block());
  PrintIndentedVisit("FINALLY", node->finally_block());
}


void AstPrinter::VisitDebuggerStatement(DebuggerStatement* node) {
  IndentedScope indent("DEBUGGER");
}


void AstPrinter::VisitFunctionLiteral(FunctionLiteral* node) {
  IndentedScope indent("FUNC LITERAL");
  PrintLiteralIndented("NAME", node->name(), false);
  PrintLiteralIndented("INFERRED NAME", node->inferred_name(), false);
  PrintParameters(node->scope());
  // We don't want to see the function literal in this case: it
  // will be printed via PrintProgram when the code for it is
  // generated.
  // PrintStatements(node->body());
}


void AstPrinter::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  IndentedScope indent("FUNC LITERAL");
  PrintLiteralIndented("BOILERPLATE", node->boilerplate(), true);
}


void AstPrinter::VisitConditional(Conditional* node) {
  IndentedScope indent("CONDITIONAL");
  PrintIndentedVisit("?", node->condition());
  PrintIndentedVisit("THEN", node->then_expression());
  PrintIndentedVisit("ELSE", node->else_expression());
}


void AstPrinter::VisitLiteral(Literal* node) {
  PrintLiteralIndented("LITERAL", node->handle(), true);
}


void AstPrinter::VisitRegExpLiteral(RegExpLiteral* node) {
  IndentedScope indent("REGEXP LITERAL");
  PrintLiteralIndented("PATTERN", node->pattern(), false);
  PrintLiteralIndented("FLAGS", node->flags(), false);
}


void AstPrinter::VisitObjectLiteral(ObjectLiteral* node) {
  IndentedScope indent("OBJ LITERAL");
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
    IndentedScope prop(prop_kind);
    PrintIndentedVisit("KEY", node->properties()->at(i)->key());
    PrintIndentedVisit("VALUE", node->properties()->at(i)->value());
  }
}


void AstPrinter::VisitArrayLiteral(ArrayLiteral* node) {
  IndentedScope indent("ARRAY LITERAL");
  if (node->values()->length() > 0) {
    IndentedScope indent("VALUES");
    for (int i = 0; i < node->values()->length(); i++) {
      Visit(node->values()->at(i));
    }
  }
}


void AstPrinter::VisitCatchExtensionObject(CatchExtensionObject* node) {
  IndentedScope indent("CatchExtensionObject");
  PrintIndentedVisit("KEY", node->key());
  PrintIndentedVisit("VALUE", node->value());
}


void AstPrinter::VisitSlot(Slot* node) {
  PrintIndented("SLOT ");
  switch (node->type()) {
    case Slot::PARAMETER:
      Print("parameter[%d]", node->index());
      break;
    case Slot::LOCAL:
      Print("frame[%d]", node->index());
      break;
    case Slot::CONTEXT:
      Print(".context[%d]", node->index());
      break;
    case Slot::LOOKUP:
      Print(".context[");
      PrintLiteral(node->var()->name(), false);
      Print("]");
      break;
    default:
      UNREACHABLE();
  }
  Print("\n");
}


void AstPrinter::VisitVariableProxy(VariableProxy* node) {
  PrintLiteralWithModeIndented("VAR PROXY", node->AsVariable(), node->name(),
                               node->type(), node->num());
  Variable* var = node->var();
  if (var != NULL && var->rewrite() != NULL) {
    IndentedScope indent;
    Visit(var->rewrite());
  }
}


void AstPrinter::VisitAssignment(Assignment* node) {
  IndentedScope indent(Token::Name(node->op()), node);
  Visit(node->target());
  Visit(node->value());
}


void AstPrinter::VisitThrow(Throw* node) {
  PrintIndentedVisit("THROW", node->exception());
}


void AstPrinter::VisitProperty(Property* node) {
  IndentedScope indent("PROPERTY", node);
  Visit(node->obj());
  Literal* literal = node->key()->AsLiteral();
  if (literal != NULL && literal->handle()->IsSymbol()) {
    PrintLiteralIndented("NAME", literal->handle(), false);
  } else {
    PrintIndentedVisit("KEY", node->key());
  }
}


void AstPrinter::VisitCall(Call* node) {
  IndentedScope indent("CALL");
  Visit(node->expression());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitCallNew(CallNew* node) {
  IndentedScope indent("CALL NEW");
  Visit(node->expression());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitCallRuntime(CallRuntime* node) {
  PrintLiteralIndented("CALL RUNTIME ", node->name(), false);
  IndentedScope indent;
  PrintArguments(node->arguments());
}


void AstPrinter::VisitUnaryOperation(UnaryOperation* node) {
  PrintIndentedVisit(Token::Name(node->op()), node->expression());
}


void AstPrinter::VisitCountOperation(CountOperation* node) {
  EmbeddedVector<char, 128> buf;
  if (node->type()->IsKnown()) {
    OS::SNPrintF(buf, "%s %s (type = %s)",
                 (node->is_prefix() ? "PRE" : "POST"),
                 Token::Name(node->op()),
                 StaticType::Type2String(node->type()));
  } else {
    OS::SNPrintF(buf, "%s %s", (node->is_prefix() ? "PRE" : "POST"),
                 Token::Name(node->op()));
  }
  PrintIndentedVisit(buf.start(), node->expression());
}


void AstPrinter::VisitBinaryOperation(BinaryOperation* node) {
  IndentedScope indent(Token::Name(node->op()), node);
  Visit(node->left());
  Visit(node->right());
}


void AstPrinter::VisitCompareOperation(CompareOperation* node) {
  IndentedScope indent(Token::Name(node->op()), node);
  Visit(node->left());
  Visit(node->right());
}


void AstPrinter::VisitThisFunction(ThisFunction* node) {
  IndentedScope indent("THIS-FUNCTION");
}


TagScope::TagScope(JsonAstBuilder* builder, const char* name)
    : builder_(builder), next_(builder->tag()), has_body_(false) {
  if (next_ != NULL) {
    next_->use();
    builder->Print(",\n");
  }
  builder->set_tag(this);
  builder->PrintIndented("[");
  builder->Print("\"%s\"", name);
  builder->increase_indent(JsonAstBuilder::kTagIndentSize);
}


TagScope::~TagScope() {
  builder_->decrease_indent(JsonAstBuilder::kTagIndentSize);
  if (has_body_) {
    builder_->Print("\n");
    builder_->PrintIndented("]");
  } else {
    builder_->Print("]");
  }
  builder_->set_tag(next_);
}


AttributesScope::AttributesScope(JsonAstBuilder* builder)
    : builder_(builder), attribute_count_(0) {
  builder->set_attributes(this);
  builder->tag()->use();
  builder->Print(",\n");
  builder->PrintIndented("{");
  builder->increase_indent(JsonAstBuilder::kAttributesIndentSize);
}


AttributesScope::~AttributesScope() {
  builder_->decrease_indent(JsonAstBuilder::kAttributesIndentSize);
  if (attribute_count_ > 1) {
    builder_->Print("\n");
    builder_->PrintIndented("}");
  } else {
    builder_->Print("}");
  }
  builder_->set_attributes(NULL);
}


const char* JsonAstBuilder::BuildProgram(FunctionLiteral* program) {
  Init();
  Visit(program);
  Print("\n");
  return Output();
}


void JsonAstBuilder::AddAttributePrefix(const char* name) {
  if (attributes()->is_used()) {
    Print(",\n");
    PrintIndented("\"");
  } else {
    Print("\"");
  }
  Print("%s\":", name);
  attributes()->use();
}


void JsonAstBuilder::AddAttribute(const char* name, Handle<String> value) {
  SmartPointer<char> value_string = value->ToCString();
  AddAttributePrefix(name);
  Print("\"%s\"", *value_string);
}


void JsonAstBuilder::AddAttribute(const char* name, const char* value) {
  AddAttributePrefix(name);
  Print("\"%s\"", value);
}


void JsonAstBuilder::AddAttribute(const char* name, int value) {
  AddAttributePrefix(name);
  Print("%d", value);
}


void JsonAstBuilder::AddAttribute(const char* name, bool value) {
  AddAttributePrefix(name);
  Print(value ? "true" : "false");
}


void JsonAstBuilder::VisitBlock(Block* stmt) {
  TagScope tag(this, "Block");
  VisitStatements(stmt->statements());
}


void JsonAstBuilder::VisitExpressionStatement(ExpressionStatement* stmt) {
  TagScope tag(this, "ExpressionStatement");
  Visit(stmt->expression());
}


void JsonAstBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  TagScope tag(this, "EmptyStatement");
}


void JsonAstBuilder::VisitIfStatement(IfStatement* stmt) {
  TagScope tag(this, "IfStatement");
  Visit(stmt->condition());
  Visit(stmt->then_statement());
  Visit(stmt->else_statement());
}


void JsonAstBuilder::VisitContinueStatement(ContinueStatement* stmt) {
  TagScope tag(this, "ContinueStatement");
}


void JsonAstBuilder::VisitBreakStatement(BreakStatement* stmt) {
  TagScope tag(this, "BreakStatement");
}


void JsonAstBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  TagScope tag(this, "ReturnStatement");
  Visit(stmt->expression());
}


void JsonAstBuilder::VisitWithEnterStatement(WithEnterStatement* stmt) {
  TagScope tag(this, "WithEnterStatement");
  Visit(stmt->expression());
}


void JsonAstBuilder::VisitWithExitStatement(WithExitStatement* stmt) {
  TagScope tag(this, "WithExitStatement");
}


void JsonAstBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  TagScope tag(this, "SwitchStatement");
}


void JsonAstBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  TagScope tag(this, "DoWhileStatement");
  Visit(stmt->body());
  Visit(stmt->cond());
}


void JsonAstBuilder::VisitWhileStatement(WhileStatement* stmt) {
  TagScope tag(this, "WhileStatement");
  Visit(stmt->cond());
  Visit(stmt->body());
}


void JsonAstBuilder::VisitForStatement(ForStatement* stmt) {
  TagScope tag(this, "ForStatement");
  if (stmt->init() != NULL) Visit(stmt->init());
  if (stmt->cond() != NULL) Visit(stmt->cond());
  Visit(stmt->body());
  if (stmt->next() != NULL) Visit(stmt->next());
}


void JsonAstBuilder::VisitForInStatement(ForInStatement* stmt) {
  TagScope tag(this, "ForInStatement");
  Visit(stmt->each());
  Visit(stmt->enumerable());
  Visit(stmt->body());
}


void JsonAstBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  TagScope tag(this, "TryCatchStatement");
  Visit(stmt->try_block());
  Visit(stmt->catch_var());
  Visit(stmt->catch_block());
}


void JsonAstBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  TagScope tag(this, "TryFinallyStatement");
  Visit(stmt->try_block());
  Visit(stmt->finally_block());
}


void JsonAstBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  TagScope tag(this, "DebuggerStatement");
}


void JsonAstBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  TagScope tag(this, "FunctionLiteral");
  {
    AttributesScope attributes(this);
    AddAttribute("name", expr->name());
  }
  VisitDeclarations(expr->scope()->declarations());
  VisitStatements(expr->body());
}


void JsonAstBuilder::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  TagScope tag(this, "FunctionBoilerplateLiteral");
}


void JsonAstBuilder::VisitConditional(Conditional* expr) {
  TagScope tag(this, "Conditional");
}


void JsonAstBuilder::VisitSlot(Slot* expr) {
  TagScope tag(this, "Slot");
  {
    AttributesScope attributes(this);
    switch (expr->type()) {
      case Slot::PARAMETER:
        AddAttribute("type", "PARAMETER");
        break;
      case Slot::LOCAL:
        AddAttribute("type", "LOCAL");
        break;
      case Slot::CONTEXT:
        AddAttribute("type", "CONTEXT");
        break;
      case Slot::LOOKUP:
        AddAttribute("type", "LOOKUP");
        break;
    }
    AddAttribute("index", expr->index());
  }
}


void JsonAstBuilder::VisitVariableProxy(VariableProxy* expr) {
  if (expr->var()->rewrite() == NULL) {
    TagScope tag(this, "VariableProxy");
    {
      AttributesScope attributes(this);
      AddAttribute("name", expr->name());
      AddAttribute("mode", Variable::Mode2String(expr->var()->mode()));
    }
  } else {
    Visit(expr->var()->rewrite());
  }
}


void JsonAstBuilder::VisitLiteral(Literal* expr) {
  TagScope tag(this, "Literal");
  {
    AttributesScope attributes(this);
    Handle<Object> handle = expr->handle();
    if (handle->IsString()) {
      AddAttribute("handle", Handle<String>(String::cast(*handle)));
    } else if (handle->IsSmi()) {
      AddAttribute("handle", Smi::cast(*handle)->value());
    }
  }
}


void JsonAstBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  TagScope tag(this, "RegExpLiteral");
}


void JsonAstBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  TagScope tag(this, "ObjectLiteral");
}


void JsonAstBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  TagScope tag(this, "ArrayLiteral");
}


void JsonAstBuilder::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  TagScope tag(this, "CatchExtensionObject");
  Visit(expr->key());
  Visit(expr->value());
}


void JsonAstBuilder::VisitAssignment(Assignment* expr) {
  TagScope tag(this, "Assignment");
  {
    AttributesScope attributes(this);
    AddAttribute("op", Token::Name(expr->op()));
  }
  Visit(expr->target());
  Visit(expr->value());
}


void JsonAstBuilder::VisitThrow(Throw* expr) {
  TagScope tag(this, "Throw");
  Visit(expr->exception());
}


void JsonAstBuilder::VisitProperty(Property* expr) {
  TagScope tag(this, "Property");
  {
    AttributesScope attributes(this);
    AddAttribute("type", expr->is_synthetic() ? "SYNTHETIC" : "NORMAL");
  }
  Visit(expr->obj());
  Visit(expr->key());
}


void JsonAstBuilder::VisitCall(Call* expr) {
  TagScope tag(this, "Call");
  Visit(expr->expression());
  VisitExpressions(expr->arguments());
}


void JsonAstBuilder::VisitCallNew(CallNew* expr) {
  TagScope tag(this, "CallNew");
  Visit(expr->expression());
  VisitExpressions(expr->arguments());
}


void JsonAstBuilder::VisitCallRuntime(CallRuntime* expr) {
  TagScope tag(this, "CallRuntime");
  {
    AttributesScope attributes(this);
    AddAttribute("name", expr->name());
  }
  VisitExpressions(expr->arguments());
}


void JsonAstBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  TagScope tag(this, "UnaryOperation");
  {
    AttributesScope attributes(this);
    AddAttribute("op", Token::Name(expr->op()));
  }
  Visit(expr->expression());
}


void JsonAstBuilder::VisitCountOperation(CountOperation* expr) {
  TagScope tag(this, "CountOperation");
  {
    AttributesScope attributes(this);
    AddAttribute("is_prefix", expr->is_prefix());
    AddAttribute("op", Token::Name(expr->op()));
  }
  Visit(expr->expression());
}


void JsonAstBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  TagScope tag(this, "BinaryOperation");
  {
    AttributesScope attributes(this);
    AddAttribute("op", Token::Name(expr->op()));
  }
  Visit(expr->left());
  Visit(expr->right());
}


void JsonAstBuilder::VisitCompareOperation(CompareOperation* expr) {
  TagScope tag(this, "CompareOperation");
  {
    AttributesScope attributes(this);
    AddAttribute("op", Token::Name(expr->op()));
  }
  Visit(expr->left());
  Visit(expr->right());
}


void JsonAstBuilder::VisitThisFunction(ThisFunction* expr) {
  TagScope tag(this, "ThisFunction");
}


void JsonAstBuilder::VisitDeclaration(Declaration* decl) {
  TagScope tag(this, "Declaration");
  {
    AttributesScope attributes(this);
    AddAttribute("mode", Variable::Mode2String(decl->mode()));
  }
  Visit(decl->proxy());
  if (decl->fun() != NULL) Visit(decl->fun());
}


#endif  // DEBUG

} }  // namespace v8::internal
