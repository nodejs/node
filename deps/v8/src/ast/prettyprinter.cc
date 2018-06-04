// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/prettyprinter.h"

#include <stdarg.h>

#include "src/ast/ast-value-factory.h"
#include "src/ast/scopes.h"
#include "src/base/platform/platform.h"
#include "src/globals.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

CallPrinter::CallPrinter(Isolate* isolate, bool is_user_js)
    : builder_(isolate) {
  isolate_ = isolate;
  position_ = 0;
  num_prints_ = 0;
  found_ = false;
  done_ = false;
  is_call_error_ = false;
  is_iterator_error_ = false;
  is_async_iterator_error_ = false;
  is_user_js_ = is_user_js;
  function_kind_ = kNormalFunction;
  InitializeAstVisitor(isolate);
}

CallPrinter::ErrorHint CallPrinter::GetErrorHint() const {
  if (is_call_error_) {
    if (is_iterator_error_) return ErrorHint::kCallAndNormalIterator;
    if (is_async_iterator_error_) return ErrorHint::kCallAndAsyncIterator;
  } else {
    if (is_iterator_error_) return ErrorHint::kNormalIterator;
    if (is_async_iterator_error_) return ErrorHint::kAsyncIterator;
  }
  return ErrorHint::kNone;
}

Handle<String> CallPrinter::Print(FunctionLiteral* program, int position) {
  num_prints_ = 0;
  position_ = position;
  Find(program);
  return builder_.Finish().ToHandleChecked();
}


void CallPrinter::Find(AstNode* node, bool print) {
  if (found_) {
    if (print) {
      int prev_num_prints = num_prints_;
      Visit(node);
      if (prev_num_prints != num_prints_) return;
    }
    Print("(intermediate value)");
  } else {
    Visit(node);
  }
}

void CallPrinter::Print(const char* str) {
  if (!found_ || done_) return;
  num_prints_++;
  builder_.AppendCString(str);
}

void CallPrinter::Print(Handle<String> str) {
  if (!found_ || done_) return;
  num_prints_++;
  builder_.AppendString(str);
}

void CallPrinter::VisitBlock(Block* node) {
  FindStatements(node->statements());
}


void CallPrinter::VisitVariableDeclaration(VariableDeclaration* node) {}


void CallPrinter::VisitFunctionDeclaration(FunctionDeclaration* node) {}


void CallPrinter::VisitExpressionStatement(ExpressionStatement* node) {
  Find(node->expression());
}


void CallPrinter::VisitEmptyStatement(EmptyStatement* node) {}


void CallPrinter::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  Find(node->statement());
}


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
  for (CaseClause* clause : *node->cases()) {
    if (!clause->is_default()) Find(clause->label());
    FindStatements(clause->statements());
  }
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
  if (node->init() != nullptr) {
    Find(node->init());
  }
  if (node->cond() != nullptr) Find(node->cond());
  if (node->next() != nullptr) Find(node->next());
  Find(node->body());
}


void CallPrinter::VisitForInStatement(ForInStatement* node) {
  Find(node->each());
  Find(node->enumerable());
  Find(node->body());
}


void CallPrinter::VisitForOfStatement(ForOfStatement* node) {
  Find(node->assign_iterator());
  Find(node->next_result());
  Find(node->result_done());
  Find(node->assign_each());
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
  FunctionKind last_function_kind = function_kind_;
  function_kind_ = node->kind();
  FindStatements(node->body());
  function_kind_ = last_function_kind;
}


void CallPrinter::VisitClassLiteral(ClassLiteral* node) {
  if (node->extends()) Find(node->extends());
  for (int i = 0; i < node->properties()->length(); i++) {
    Find(node->properties()->at(i)->value());
  }
}

void CallPrinter::VisitInitializeClassFieldsStatement(
    InitializeClassFieldsStatement* node) {
  for (int i = 0; i < node->fields()->length(); i++) {
    Find(node->fields()->at(i)->value());
  }
}

void CallPrinter::VisitNativeFunctionLiteral(NativeFunctionLiteral* node) {}


void CallPrinter::VisitDoExpression(DoExpression* node) { Find(node->block()); }


void CallPrinter::VisitConditional(Conditional* node) {
  Find(node->condition());
  Find(node->then_expression());
  Find(node->else_expression());
}


void CallPrinter::VisitLiteral(Literal* node) {
  // TODO(adamk): Teach Literal how to print its values without
  // allocating on the heap.
  PrintLiteral(node->BuildValue(isolate_), true);
}


void CallPrinter::VisitRegExpLiteral(RegExpLiteral* node) {
  Print("/");
  PrintLiteral(node->pattern(), false);
  Print("/");
  if (node->flags() & RegExp::kGlobal) Print("g");
  if (node->flags() & RegExp::kIgnoreCase) Print("i");
  if (node->flags() & RegExp::kMultiline) Print("m");
  if (node->flags() & RegExp::kUnicode) Print("u");
  if (node->flags() & RegExp::kSticky) Print("y");
}


void CallPrinter::VisitObjectLiteral(ObjectLiteral* node) {
  Print("{");
  for (int i = 0; i < node->properties()->length(); i++) {
    Find(node->properties()->at(i)->value());
  }
  Print("}");
}


void CallPrinter::VisitArrayLiteral(ArrayLiteral* node) {
  Print("[");
  for (int i = 0; i < node->values()->length(); i++) {
    if (i != 0) Print(",");
    Expression* subexpr = node->values()->at(i);
    Spread* spread = subexpr->AsSpread();
    if (spread != nullptr && !found_ &&
        position_ == spread->expression()->position()) {
      found_ = true;
      is_iterator_error_ = true;
      Find(spread->expression(), true);
      done_ = true;
      return;
    }
    Find(subexpr, true);
  }
  Print("]");
}


void CallPrinter::VisitVariableProxy(VariableProxy* node) {
  if (is_user_js_) {
    PrintLiteral(node->name(), false);
  } else {
    // Variable names of non-user code are meaningless due to minification.
    Print("(var)");
  }
}


void CallPrinter::VisitAssignment(Assignment* node) {
  Find(node->target());
  Find(node->value());
}

void CallPrinter::VisitCompoundAssignment(CompoundAssignment* node) {
  VisitAssignment(node);
}

void CallPrinter::VisitYield(Yield* node) { Find(node->expression()); }

void CallPrinter::VisitYieldStar(YieldStar* node) {
  if (!found_ && position_ == node->expression()->position()) {
    found_ = true;
    if (IsAsyncFunction(function_kind_))
      is_async_iterator_error_ = true;
    else
      is_iterator_error_ = true;
    Print("yield* ");
  }
  Find(node->expression());
}

void CallPrinter::VisitAwait(Await* node) { Find(node->expression()); }

void CallPrinter::VisitThrow(Throw* node) { Find(node->exception()); }


void CallPrinter::VisitProperty(Property* node) {
  Expression* key = node->key();
  Literal* literal = key->AsLiteral();
  if (literal != nullptr &&
      literal->BuildValue(isolate_)->IsInternalizedString()) {
    Find(node->obj(), true);
    Print(".");
    // TODO(adamk): Teach Literal how to print its values without
    // allocating on the heap.
    PrintLiteral(literal->BuildValue(isolate_), false);
  } else {
    Find(node->obj(), true);
    Print("[");
    Find(key, true);
    Print("]");
  }
}

void CallPrinter::VisitResolvedProperty(ResolvedProperty* node) {}

void CallPrinter::VisitCall(Call* node) {
  bool was_found = false;
  if (node->position() == position_) {
    is_call_error_ = true;
    was_found = !found_;
  }
  if (was_found) {
    // Bail out if the error is caused by a direct call to a variable in
    // non-user JS code. The variable name is meaningless due to minification.
    if (!is_user_js_ && node->expression()->IsVariableProxy()) {
      done_ = true;
      return;
    }
    found_ = true;
  }
  Find(node->expression(), true);
  if (!was_found) Print("(...)");
  FindArguments(node->arguments());
  if (was_found) {
    done_ = true;
    found_ = false;
  }
}


void CallPrinter::VisitCallNew(CallNew* node) {
  bool was_found = false;
  if (node->position() == position_) {
    is_call_error_ = true;
    was_found = !found_;
  }
  if (was_found) {
    // Bail out if the error is caused by a direct call to a variable in
    // non-user JS code. The variable name is meaningless due to minification.
    if (!is_user_js_ && node->expression()->IsVariableProxy()) {
      done_ = true;
      return;
    }
    found_ = true;
  }
  Find(node->expression(), was_found);
  FindArguments(node->arguments());
  if (was_found) {
    done_ = true;
    found_ = false;
  }
}


void CallPrinter::VisitCallRuntime(CallRuntime* node) {
  FindArguments(node->arguments());
}


void CallPrinter::VisitUnaryOperation(UnaryOperation* node) {
  Token::Value op = node->op();
  bool needsSpace =
      op == Token::DELETE || op == Token::TYPEOF || op == Token::VOID;
  Print("(");
  Print(Token::String(op));
  if (needsSpace) Print(" ");
  Find(node->expression(), true);
  Print(")");
}


void CallPrinter::VisitCountOperation(CountOperation* node) {
  Print("(");
  if (node->is_prefix()) Print(Token::String(node->op()));
  Find(node->expression(), true);
  if (node->is_postfix()) Print(Token::String(node->op()));
  Print(")");
}


void CallPrinter::VisitBinaryOperation(BinaryOperation* node) {
  Print("(");
  Find(node->left(), true);
  Print(" ");
  Print(Token::String(node->op()));
  Print(" ");
  Find(node->right(), true);
  Print(")");
}

void CallPrinter::VisitNaryOperation(NaryOperation* node) {
  Print("(");
  Find(node->first(), true);
  for (size_t i = 0; i < node->subsequent_length(); ++i) {
    Print(" ");
    Print(Token::String(node->op()));
    Print(" ");
    Find(node->subsequent(i), true);
  }
  Print(")");
}

void CallPrinter::VisitCompareOperation(CompareOperation* node) {
  Print("(");
  Find(node->left(), true);
  Print(" ");
  Print(Token::String(node->op()));
  Print(" ");
  Find(node->right(), true);
  Print(")");
}


void CallPrinter::VisitSpread(Spread* node) {
  Print("(...");
  Find(node->expression(), true);
  Print(")");
}

void CallPrinter::VisitStoreInArrayLiteral(StoreInArrayLiteral* node) {
  Find(node->array());
  Find(node->index());
  Find(node->value());
}

void CallPrinter::VisitEmptyParentheses(EmptyParentheses* node) {
  UNREACHABLE();
}

void CallPrinter::VisitGetIterator(GetIterator* node) {
  bool was_found = false;
  if (node->position() == position_) {
    is_async_iterator_error_ = node->hint() == IteratorType::kAsync;
    is_iterator_error_ = !is_async_iterator_error_;
    was_found = !found_;
    if (was_found) {
      found_ = true;
    }
  }
  Find(node->iterable_for_call_printer(), true);
  if (was_found) {
    done_ = true;
    found_ = false;
  }
}

void CallPrinter::VisitGetTemplateObject(GetTemplateObject* node) {}

void CallPrinter::VisitTemplateLiteral(TemplateLiteral* node) {
  for (Expression* substitution : *node->substitutions()) {
    Find(substitution, true);
  }
}

void CallPrinter::VisitImportCallExpression(ImportCallExpression* node) {
  Print("ImportCall(");
  Find(node->argument(), true);
  Print(")");
}

void CallPrinter::VisitThisFunction(ThisFunction* node) {}


void CallPrinter::VisitSuperPropertyReference(SuperPropertyReference* node) {}


void CallPrinter::VisitSuperCallReference(SuperCallReference* node) {
  Print("super");
}


void CallPrinter::VisitRewritableExpression(RewritableExpression* node) {
  Find(node->expression());
}


void CallPrinter::FindStatements(ZoneList<Statement*>* statements) {
  if (statements == nullptr) return;
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
  if (value->IsString()) {
    if (quote) Print("\"");
    Print(Handle<String>::cast(value));
    if (quote) Print("\"");
  } else if (value->IsNull(isolate_)) {
    Print("null");
  } else if (value->IsTrue(isolate_)) {
    Print("true");
  } else if (value->IsFalse(isolate_)) {
    Print("false");
  } else if (value->IsUndefined(isolate_)) {
    Print("undefined");
  } else if (value->IsNumber()) {
    Print(isolate_->factory()->NumberToString(value));
  } else if (value->IsSymbol()) {
    // Symbols can only occur as literals if they were inserted by the parser.
    PrintLiteral(handle(Handle<Symbol>::cast(value)->name(), isolate_), false);
  }
}


void CallPrinter::PrintLiteral(const AstRawString* value, bool quote) {
  PrintLiteral(value->string(), quote);
}


//-----------------------------------------------------------------------------


#ifdef DEBUG

const char* AstPrinter::Print(AstNode* node) {
  Init();
  Visit(node);
  return output_;
}

void AstPrinter::Init() {
  if (size_ == 0) {
    DCHECK_NULL(output_);
    const int initial_size = 256;
    output_ = NewArray<char>(initial_size);
    size_ = initial_size;
  }
  output_[0] = '\0';
  pos_ = 0;
}

void AstPrinter::Print(const char* format, ...) {
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

void AstPrinter::PrintLabels(ZoneList<const AstRawString*>* labels) {
  if (labels != nullptr) {
    for (int i = 0; i < labels->length(); i++) {
      PrintLiteral(labels->at(i), false);
      Print(": ");
    }
  }
}

void AstPrinter::PrintLiteral(Literal* literal, bool quote) {
  switch (literal->type()) {
    case Literal::kString:
      PrintLiteral(literal->AsRawString(), quote);
      break;
    case Literal::kSymbol:
      const char* symbol;
      switch (literal->AsSymbol()) {
        case AstSymbol::kHomeObjectSymbol:
          symbol = "HomeObjectSymbol";
      }
      Print("%s", symbol);
      break;
    case Literal::kSmi:
      Print("%d", Smi::ToInt(literal->AsSmiLiteral()));
      break;
    case Literal::kHeapNumber:
      Print("%g", literal->AsNumber());
      break;
    case Literal::kBigInt:
      Print("%sn", literal->AsBigInt().c_str());
      break;
    case Literal::kNull:
      Print("null");
      break;
    case Literal::kUndefined:
      Print("undefined");
      break;
    case Literal::kTheHole:
      Print("the hole");
      break;
    case Literal::kBoolean:
      if (literal->ToBooleanIsTrue()) {
        Print("true");
      } else {
        Print("false");
      }
      break;
  }
}

void AstPrinter::PrintLiteral(const AstRawString* value, bool quote) {
  if (quote) Print("\"");
  if (value != nullptr) {
    const char* format = value->is_one_byte() ? "%c" : "%lc";
    const int increment = value->is_one_byte() ? 1 : 2;
    const unsigned char* raw_bytes = value->raw_data();
    for (int i = 0; i < value->length(); i += increment) {
      Print(format, raw_bytes[i]);
    }
  }
  if (quote) Print("\"");
}

void AstPrinter::PrintLiteral(const AstConsString* value, bool quote) {
  if (quote) Print("\"");
  if (value != nullptr) {
    std::forward_list<const AstRawString*> strings = value->ToRawStrings();
    for (const AstRawString* string : strings) {
      PrintLiteral(string, false);
    }
  }
  if (quote) Print("\"");
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

  IndentedScope(AstPrinter* printer, const char* txt, int pos)
      : ast_printer_(printer) {
    ast_printer_->PrintIndented(txt);
    ast_printer_->Print(" at %d\n", pos);
    ast_printer_->inc_indent();
  }

  virtual ~IndentedScope() {
    ast_printer_->dec_indent();
  }

 private:
  AstPrinter* ast_printer_;
};


//-----------------------------------------------------------------------------

AstPrinter::AstPrinter(uintptr_t stack_limit)
    : output_(nullptr), size_(0), pos_(0), indent_(0) {
  InitializeAstVisitor(stack_limit);
}

AstPrinter::~AstPrinter() {
  DCHECK_EQ(indent_, 0);
  DeleteArray(output_);
}


void AstPrinter::PrintIndented(const char* txt) {
  for (int i = 0; i < indent_; i++) {
    Print(". ");
  }
  Print("%s", txt);
}

void AstPrinter::PrintLiteralIndented(const char* info, Literal* literal,
                                      bool quote) {
  PrintIndented(info);
  Print(" ");
  PrintLiteral(literal, quote);
  Print("\n");
}

void AstPrinter::PrintLiteralIndented(const char* info,
                                      const AstRawString* value, bool quote) {
  PrintIndented(info);
  Print(" ");
  PrintLiteral(value, quote);
  Print("\n");
}

void AstPrinter::PrintLiteralIndented(const char* info,
                                      const AstConsString* value, bool quote) {
  PrintIndented(info);
  Print(" ");
  PrintLiteral(value, quote);
  Print("\n");
}

void AstPrinter::PrintLiteralWithModeIndented(const char* info, Variable* var,
                                              const AstRawString* value) {
  if (var == nullptr) {
    PrintLiteralIndented(info, value, true);
  } else {
    EmbeddedVector<char, 256> buf;
    int pos =
        SNPrintF(buf, "%s (%p) (mode = %s", info, reinterpret_cast<void*>(var),
                 VariableMode2String(var->mode()));
    SNPrintF(buf + pos, ")");
    PrintLiteralIndented(buf.start(), value, true);
  }
}


void AstPrinter::PrintLabelsIndented(ZoneList<const AstRawString*>* labels) {
  if (labels == nullptr || labels->length() == 0) return;
  PrintIndented("LABELS ");
  PrintLabels(labels);
  Print("\n");
}


void AstPrinter::PrintIndentedVisit(const char* s, AstNode* node) {
  IndentedScope indent(this, s, node->position());
  Visit(node);
}


const char* AstPrinter::PrintProgram(FunctionLiteral* program) {
  Init();
  { IndentedScope indent(this, "FUNC", program->position());
    PrintIndented("KIND");
    Print(" %d\n", program->kind());
    PrintIndented("SUSPEND COUNT");
    Print(" %d\n", program->suspend_count());
    PrintLiteralIndented("NAME", program->raw_name(), true);
    if (program->raw_inferred_name()) {
      PrintLiteralIndented("INFERRED NAME", program->raw_inferred_name(), true);
    }
    if (program->requires_instance_fields_initializer()) {
      Print(" REQUIRES INSTANCE FIELDS INITIALIZER\n");
    }
    PrintParameters(program->scope());
    PrintDeclarations(program->scope()->declarations());
    PrintStatements(program->body());
  }
  return output_;
}


void AstPrinter::PrintOut(Isolate* isolate, AstNode* node) {
  AstPrinter printer(isolate->stack_guard()->real_climit());
  printer.Init();
  printer.Visit(node);
  PrintF("%s", printer.output_);
}

void AstPrinter::PrintDeclarations(Declaration::List* declarations) {
  if (!declarations->is_empty()) {
    IndentedScope indent(this, "DECLS");
    for (Declaration* decl : *declarations) Visit(decl);
  }
}

void AstPrinter::PrintParameters(DeclarationScope* scope) {
  if (scope->num_parameters() > 0) {
    IndentedScope indent(this, "PARAMS");
    for (int i = 0; i < scope->num_parameters(); i++) {
      PrintLiteralWithModeIndented("VAR", scope->parameter(i),
                                   scope->parameter(i)->raw_name());
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
  const char* block_txt =
      node->ignore_completion_value() ? "BLOCK NOCOMPLETIONS" : "BLOCK";
  IndentedScope indent(this, block_txt, node->position());
  PrintStatements(node->statements());
}


// TODO(svenpanne) Start with IndentedScope.
void AstPrinter::VisitVariableDeclaration(VariableDeclaration* node) {
  PrintLiteralWithModeIndented("VARIABLE", node->proxy()->var(),
                               node->proxy()->raw_name());
}


// TODO(svenpanne) Start with IndentedScope.
void AstPrinter::VisitFunctionDeclaration(FunctionDeclaration* node) {
  PrintIndented("FUNCTION ");
  PrintLiteral(node->proxy()->raw_name(), true);
  Print(" = function ");
  PrintLiteral(node->fun()->raw_name(), false);
  Print("\n");
}


void AstPrinter::VisitExpressionStatement(ExpressionStatement* node) {
  IndentedScope indent(this, "EXPRESSION STATEMENT", node->position());
  Visit(node->expression());
}


void AstPrinter::VisitEmptyStatement(EmptyStatement* node) {
  IndentedScope indent(this, "EMPTY", node->position());
}


void AstPrinter::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  Visit(node->statement());
}


void AstPrinter::VisitIfStatement(IfStatement* node) {
  IndentedScope indent(this, "IF", node->position());
  PrintIndentedVisit("CONDITION", node->condition());
  PrintIndentedVisit("THEN", node->then_statement());
  if (node->HasElseStatement()) {
    PrintIndentedVisit("ELSE", node->else_statement());
  }
}


void AstPrinter::VisitContinueStatement(ContinueStatement* node) {
  IndentedScope indent(this, "CONTINUE", node->position());
  PrintLabelsIndented(node->target()->labels());
}


void AstPrinter::VisitBreakStatement(BreakStatement* node) {
  IndentedScope indent(this, "BREAK", node->position());
  PrintLabelsIndented(node->target()->labels());
}


void AstPrinter::VisitReturnStatement(ReturnStatement* node) {
  IndentedScope indent(this, "RETURN", node->position());
  Visit(node->expression());
}


void AstPrinter::VisitWithStatement(WithStatement* node) {
  IndentedScope indent(this, "WITH", node->position());
  PrintIndentedVisit("OBJECT", node->expression());
  PrintIndentedVisit("BODY", node->statement());
}


void AstPrinter::VisitSwitchStatement(SwitchStatement* node) {
  IndentedScope indent(this, "SWITCH", node->position());
  PrintLabelsIndented(node->labels());
  PrintIndentedVisit("TAG", node->tag());
  for (CaseClause* clause : *node->cases()) {
    if (clause->is_default()) {
      IndentedScope indent(this, "DEFAULT");
      PrintStatements(clause->statements());
    } else {
      IndentedScope indent(this, "CASE");
      Visit(clause->label());
      PrintStatements(clause->statements());
    }
  }
}


void AstPrinter::VisitDoWhileStatement(DoWhileStatement* node) {
  IndentedScope indent(this, "DO", node->position());
  PrintLabelsIndented(node->labels());
  PrintIndentedVisit("BODY", node->body());
  PrintIndentedVisit("COND", node->cond());
}


void AstPrinter::VisitWhileStatement(WhileStatement* node) {
  IndentedScope indent(this, "WHILE", node->position());
  PrintLabelsIndented(node->labels());
  PrintIndentedVisit("COND", node->cond());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitForStatement(ForStatement* node) {
  IndentedScope indent(this, "FOR", node->position());
  PrintLabelsIndented(node->labels());
  if (node->init()) PrintIndentedVisit("INIT", node->init());
  if (node->cond()) PrintIndentedVisit("COND", node->cond());
  PrintIndentedVisit("BODY", node->body());
  if (node->next()) PrintIndentedVisit("NEXT", node->next());
}


void AstPrinter::VisitForInStatement(ForInStatement* node) {
  IndentedScope indent(this, "FOR IN", node->position());
  PrintIndentedVisit("FOR", node->each());
  PrintIndentedVisit("IN", node->enumerable());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitForOfStatement(ForOfStatement* node) {
  IndentedScope indent(this, "FOR OF", node->position());
  PrintIndentedVisit("INIT", node->assign_iterator());
  PrintIndentedVisit("NEXT", node->next_result());
  PrintIndentedVisit("DONE", node->result_done());
  PrintIndentedVisit("EACH", node->assign_each());
  PrintIndentedVisit("BODY", node->body());
}


void AstPrinter::VisitTryCatchStatement(TryCatchStatement* node) {
  IndentedScope indent(this, "TRY CATCH", node->position());
  PrintIndentedVisit("TRY", node->try_block());
  PrintIndented("CATCH PREDICTION");
  const char* prediction = "";
  switch (node->GetCatchPrediction(HandlerTable::UNCAUGHT)) {
    case HandlerTable::UNCAUGHT:
      prediction = "UNCAUGHT";
      break;
    case HandlerTable::CAUGHT:
      prediction = "CAUGHT";
      break;
    case HandlerTable::DESUGARING:
      prediction = "DESUGARING";
      break;
    case HandlerTable::ASYNC_AWAIT:
      prediction = "ASYNC_AWAIT";
      break;
    case HandlerTable::PROMISE:
      // Catch prediction resulting in promise rejections aren't
      // parsed by the parser.
      UNREACHABLE();
  }
  Print(" %s\n", prediction);
  if (node->scope()) {
    PrintLiteralWithModeIndented("CATCHVAR", node->scope()->catch_variable(),
                                 node->scope()->catch_variable()->raw_name());
  }
  PrintIndentedVisit("CATCH", node->catch_block());
}

void AstPrinter::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IndentedScope indent(this, "TRY FINALLY", node->position());
  PrintIndentedVisit("TRY", node->try_block());
  PrintIndentedVisit("FINALLY", node->finally_block());
}

void AstPrinter::VisitDebuggerStatement(DebuggerStatement* node) {
  IndentedScope indent(this, "DEBUGGER", node->position());
}


void AstPrinter::VisitFunctionLiteral(FunctionLiteral* node) {
  IndentedScope indent(this, "FUNC LITERAL", node->position());
  PrintLiteralIndented("NAME", node->raw_name(), false);
  PrintLiteralIndented("INFERRED NAME", node->raw_inferred_name(), false);
  PrintParameters(node->scope());
  // We don't want to see the function literal in this case: it
  // will be printed via PrintProgram when the code for it is
  // generated.
  // PrintStatements(node->body());
}


void AstPrinter::VisitClassLiteral(ClassLiteral* node) {
  IndentedScope indent(this, "CLASS LITERAL", node->position());
  PrintLiteralIndented("NAME", node->constructor()->raw_name(), false);
  if (node->extends() != nullptr) {
    PrintIndentedVisit("EXTENDS", node->extends());
  }
  if (node->static_fields_initializer() != nullptr) {
    PrintIndentedVisit("STATIC FIELDS INITIALIZER",
                       node->static_fields_initializer());
  }
  if (node->instance_fields_initializer_function() != nullptr) {
    PrintIndentedVisit("INSTANCE FIELDS INITIALIZER",
                       node->instance_fields_initializer_function());
  }
  PrintClassProperties(node->properties());
}

void AstPrinter::VisitInitializeClassFieldsStatement(
    InitializeClassFieldsStatement* node) {
  IndentedScope indent(this, "INITIALIZE CLASS FIELDS", node->position());
  PrintClassProperties(node->fields());
}

void AstPrinter::PrintClassProperties(
    ZoneList<ClassLiteral::Property*>* properties) {
  for (int i = 0; i < properties->length(); i++) {
    ClassLiteral::Property* property = properties->at(i);
    const char* prop_kind = nullptr;
    switch (property->kind()) {
      case ClassLiteral::Property::METHOD:
        prop_kind = "METHOD";
        break;
      case ClassLiteral::Property::GETTER:
        prop_kind = "GETTER";
        break;
      case ClassLiteral::Property::SETTER:
        prop_kind = "SETTER";
        break;
      case ClassLiteral::Property::PUBLIC_FIELD:
        prop_kind = "PUBLIC FIELD";
        break;
      case ClassLiteral::Property::PRIVATE_FIELD:
        prop_kind = "PRIVATE FIELD";
        break;
    }
    EmbeddedVector<char, 128> buf;
    SNPrintF(buf, "PROPERTY%s - %s", property->is_static() ? " - STATIC" : "",
             prop_kind);
    IndentedScope prop(this, buf.start());
    PrintIndentedVisit("KEY", properties->at(i)->key());
    PrintIndentedVisit("VALUE", properties->at(i)->value());
  }
}


void AstPrinter::VisitNativeFunctionLiteral(NativeFunctionLiteral* node) {
  IndentedScope indent(this, "NATIVE FUNC LITERAL", node->position());
  PrintLiteralIndented("NAME", node->raw_name(), false);
}


void AstPrinter::VisitDoExpression(DoExpression* node) {
  IndentedScope indent(this, "DO EXPRESSION", node->position());
  PrintStatements(node->block()->statements());
}


void AstPrinter::VisitConditional(Conditional* node) {
  IndentedScope indent(this, "CONDITIONAL", node->position());
  PrintIndentedVisit("CONDITION", node->condition());
  PrintIndentedVisit("THEN", node->then_expression());
  PrintIndentedVisit("ELSE", node->else_expression());
}


void AstPrinter::VisitLiteral(Literal* node) {
  PrintLiteralIndented("LITERAL", node, true);
}


void AstPrinter::VisitRegExpLiteral(RegExpLiteral* node) {
  IndentedScope indent(this, "REGEXP LITERAL", node->position());
  PrintLiteralIndented("PATTERN", node->raw_pattern(), false);
  int i = 0;
  EmbeddedVector<char, 128> buf;
  if (node->flags() & RegExp::kGlobal) buf[i++] = 'g';
  if (node->flags() & RegExp::kIgnoreCase) buf[i++] = 'i';
  if (node->flags() & RegExp::kMultiline) buf[i++] = 'm';
  if (node->flags() & RegExp::kUnicode) buf[i++] = 'u';
  if (node->flags() & RegExp::kSticky) buf[i++] = 'y';
  buf[i] = '\0';
  PrintIndented("FLAGS ");
  Print("%s", buf.start());
  Print("\n");
}


void AstPrinter::VisitObjectLiteral(ObjectLiteral* node) {
  IndentedScope indent(this, "OBJ LITERAL", node->position());
  PrintObjectProperties(node->properties());
}

void AstPrinter::PrintObjectProperties(
    ZoneList<ObjectLiteral::Property*>* properties) {
  for (int i = 0; i < properties->length(); i++) {
    ObjectLiteral::Property* property = properties->at(i);
    const char* prop_kind = nullptr;
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        prop_kind = "CONSTANT";
        break;
      case ObjectLiteral::Property::COMPUTED:
        prop_kind = "COMPUTED";
        break;
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        prop_kind = "MATERIALIZED_LITERAL";
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        prop_kind = "PROTOTYPE";
        break;
      case ObjectLiteral::Property::GETTER:
        prop_kind = "GETTER";
        break;
      case ObjectLiteral::Property::SETTER:
        prop_kind = "SETTER";
        break;
      case ObjectLiteral::Property::SPREAD:
        prop_kind = "SPREAD";
        break;
    }
    EmbeddedVector<char, 128> buf;
    SNPrintF(buf, "PROPERTY - %s", prop_kind);
    IndentedScope prop(this, buf.start());
    PrintIndentedVisit("KEY", properties->at(i)->key());
    PrintIndentedVisit("VALUE", properties->at(i)->value());
  }
}


void AstPrinter::VisitArrayLiteral(ArrayLiteral* node) {
  IndentedScope indent(this, "ARRAY LITERAL", node->position());
  if (node->values()->length() > 0) {
    IndentedScope indent(this, "VALUES", node->position());
    for (int i = 0; i < node->values()->length(); i++) {
      Visit(node->values()->at(i));
    }
  }
}


void AstPrinter::VisitVariableProxy(VariableProxy* node) {
  EmbeddedVector<char, 128> buf;
  int pos = SNPrintF(buf, "VAR PROXY");

  if (!node->is_resolved()) {
    SNPrintF(buf + pos, " unresolved");
    PrintLiteralWithModeIndented(buf.start(), nullptr, node->raw_name());
  } else {
    Variable* var = node->var();
    switch (var->location()) {
      case VariableLocation::UNALLOCATED:
        SNPrintF(buf + pos, " unallocated");
        break;
      case VariableLocation::PARAMETER:
        SNPrintF(buf + pos, " parameter[%d]", var->index());
        break;
      case VariableLocation::LOCAL:
        SNPrintF(buf + pos, " local[%d]", var->index());
        break;
      case VariableLocation::CONTEXT:
        SNPrintF(buf + pos, " context[%d]", var->index());
        break;
      case VariableLocation::LOOKUP:
        SNPrintF(buf + pos, " lookup");
        break;
      case VariableLocation::MODULE:
        SNPrintF(buf + pos, " module");
        break;
    }
    PrintLiteralWithModeIndented(buf.start(), var, node->raw_name());
  }
}


void AstPrinter::VisitAssignment(Assignment* node) {
  IndentedScope indent(this, Token::Name(node->op()), node->position());
  Visit(node->target());
  Visit(node->value());
}

void AstPrinter::VisitCompoundAssignment(CompoundAssignment* node) {
  VisitAssignment(node);
}

void AstPrinter::VisitYield(Yield* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "YIELD");
  IndentedScope indent(this, buf.start(), node->position());
  Visit(node->expression());
}

void AstPrinter::VisitYieldStar(YieldStar* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "YIELD_STAR");
  IndentedScope indent(this, buf.start(), node->position());
  Visit(node->expression());
}

void AstPrinter::VisitAwait(Await* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "AWAIT");
  IndentedScope indent(this, buf.start(), node->position());
  Visit(node->expression());
}

void AstPrinter::VisitThrow(Throw* node) {
  IndentedScope indent(this, "THROW", node->position());
  Visit(node->exception());
}

void AstPrinter::VisitProperty(Property* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "PROPERTY");
  IndentedScope indent(this, buf.start(), node->position());

  Visit(node->obj());
  LhsKind property_kind = Property::GetAssignType(node);
  if (property_kind == NAMED_PROPERTY ||
      property_kind == NAMED_SUPER_PROPERTY) {
    PrintLiteralIndented("NAME", node->key()->AsLiteral(), false);
  } else {
    DCHECK(property_kind == KEYED_PROPERTY ||
           property_kind == KEYED_SUPER_PROPERTY);
    PrintIndentedVisit("KEY", node->key());
  }
}

void AstPrinter::VisitResolvedProperty(ResolvedProperty* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "RESOLVED-PROPERTY");
  IndentedScope indent(this, buf.start(), node->position());

  PrintIndentedVisit("RECEIVER", node->object());
  PrintIndentedVisit("PROPERTY", node->property());
}

void AstPrinter::VisitCall(Call* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "CALL");
  IndentedScope indent(this, buf.start());

  Visit(node->expression());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitCallNew(CallNew* node) {
  IndentedScope indent(this, "CALL NEW", node->position());
  Visit(node->expression());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitCallRuntime(CallRuntime* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "CALL RUNTIME %s%s", node->debug_name(),
           node->is_jsruntime() ? " (JS function)" : "");
  IndentedScope indent(this, buf.start(), node->position());
  PrintArguments(node->arguments());
}


void AstPrinter::VisitUnaryOperation(UnaryOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()), node->position());
  Visit(node->expression());
}


void AstPrinter::VisitCountOperation(CountOperation* node) {
  EmbeddedVector<char, 128> buf;
  SNPrintF(buf, "%s %s", (node->is_prefix() ? "PRE" : "POST"),
           Token::Name(node->op()));
  IndentedScope indent(this, buf.start(), node->position());
  Visit(node->expression());
}


void AstPrinter::VisitBinaryOperation(BinaryOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()), node->position());
  Visit(node->left());
  Visit(node->right());
}

void AstPrinter::VisitNaryOperation(NaryOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()), node->position());
  Visit(node->first());
  for (size_t i = 0; i < node->subsequent_length(); ++i) {
    Visit(node->subsequent(i));
  }
}

void AstPrinter::VisitCompareOperation(CompareOperation* node) {
  IndentedScope indent(this, Token::Name(node->op()), node->position());
  Visit(node->left());
  Visit(node->right());
}


void AstPrinter::VisitSpread(Spread* node) {
  IndentedScope indent(this, "...", node->position());
  Visit(node->expression());
}

void AstPrinter::VisitStoreInArrayLiteral(StoreInArrayLiteral* node) {
  IndentedScope indent(this, "STORE IN ARRAY LITERAL", node->position());
  PrintIndentedVisit("ARRAY", node->array());
  PrintIndentedVisit("INDEX", node->index());
  PrintIndentedVisit("VALUE", node->value());
}

void AstPrinter::VisitEmptyParentheses(EmptyParentheses* node) {
  IndentedScope indent(this, "()", node->position());
}

void AstPrinter::VisitGetIterator(GetIterator* node) {
  IndentedScope indent(this, "GET-ITERATOR", node->position());
  Visit(node->iterable());
}

void AstPrinter::VisitGetTemplateObject(GetTemplateObject* node) {
  IndentedScope indent(this, "GET-TEMPLATE-OBJECT", node->position());
}

void AstPrinter::VisitTemplateLiteral(TemplateLiteral* node) {
  IndentedScope indent(this, "TEMPLATE-LITERAL", node->position());
  const AstRawString* string = node->string_parts()->first();
  if (!string->IsEmpty()) PrintLiteralIndented("SPAN", string, true);
  for (int i = 0; i < node->substitutions()->length();) {
    PrintIndentedVisit("EXPR", node->substitutions()->at(i++));
    if (i < node->string_parts()->length()) {
      string = node->string_parts()->at(i);
      if (!string->IsEmpty()) PrintLiteralIndented("SPAN", string, true);
    }
  }
}

void AstPrinter::VisitImportCallExpression(ImportCallExpression* node) {
  IndentedScope indent(this, "IMPORT-CALL", node->position());
  Visit(node->argument());
}

void AstPrinter::VisitThisFunction(ThisFunction* node) {
  IndentedScope indent(this, "THIS-FUNCTION", node->position());
}


void AstPrinter::VisitSuperPropertyReference(SuperPropertyReference* node) {
  IndentedScope indent(this, "SUPER-PROPERTY-REFERENCE", node->position());
}


void AstPrinter::VisitSuperCallReference(SuperCallReference* node) {
  IndentedScope indent(this, "SUPER-CALL-REFERENCE", node->position());
}


void AstPrinter::VisitRewritableExpression(RewritableExpression* node) {
  Visit(node->expression());
}


#endif  // DEBUG

}  // namespace internal
}  // namespace v8
