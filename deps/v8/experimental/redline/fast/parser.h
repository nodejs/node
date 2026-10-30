// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_PARSER_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_PARSER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/lexer.h"
#include "experimental/redline/fast/tree.h"

namespace redline {
namespace fast {

struct ParseOptions {
  // Accept `%RuntimeFunction(...)` calls (d8 --allow-natives-syntax).
  bool allow_natives_syntax = true;
  // Parse as strict code from the start.
  bool strict = false;
};

struct ParseError {
  std::string message;
  int32_t position = -1;  // byte offset
  int line = 0;           // 1-based
  int column = 0;         // 1-based, in bytes

  std::string ToString() const;
};

// Computes line/column for a byte offset.
void ComputeLineColumn(std::string_view source, int32_t position, int* line,
                       int* column);

// A recursive-descent parser for ECMAScript scripts (ES2024 plus the V8
// extensions used by tests: natives syntax, `using` declarations, HTML
// comments). It is deliberately lenient about early errors that do not
// affect the tree shape (the target engine reports those); it must reject
// what it cannot represent and must never produce a tree that prints to a
// program with different semantics than the input.
class Parser {
 public:
  // Parses a script. Returns nullptr and fills `error` on failure.
  static std::unique_ptr<Tree> ParseScript(std::string_view source,
                                           const ParseOptions& options,
                                           ParseError* error);

  // Parses a single expression (for the shell). Returns nullptr on failure.
  static Expression* ParseExpressionInto(Tree* tree, std::string_view source,
                                         const ParseOptions& options,
                                         ParseError* error);

 private:
  struct FunctionState {
    bool is_async = false;
    bool is_generator = false;
    bool is_arrow = false;
    bool in_function = false;  // false at the top level
    bool in_class_field_init = false;
    bool in_static_block = false;
    bool strict = false;
  };

  Parser(Tree* tree, std::string_view source, const ParseOptions& options);

  // --- token helpers ---
  const Token& cur() const { return lexer_.current(); }
  Tok peek_type() const { return lexer_.current().type; }
  bool Check(Tok type) const { return peek_type() == type; }
  bool CheckName(std::string_view name) const {
    return peek_type() == Tok::kName && !cur().has_escape &&
           cur().value == name;
  }
  void Advance();
  bool Eat(Tok type);
  bool EatName(std::string_view name);
  bool Expect(Tok type);
  bool ExpectName(std::string_view name);
  bool ConsumeSemicolon();
  bool CanInsertSemicolon() const;
  // Looks at the token after the current one without consuming anything.
  Token PeekNext();
  Token PeekNext2();

  // Errors.
  bool Fail(const std::string& message, int32_t position);
  bool FailUnexpected();
  bool has_error() const { return failed_; }

  int32_t start_pos() const { return cur().start; }
  int32_t prev_end() const { return prev_token_end_; }
  // True if `e` is an arrow function that was not wrapped in parentheses:
  // nothing (no operator, subscript or `?`) may follow it. A parenthesized
  // arrow ends before the `)` that was consumed after it.
  bool IsBareArrow(const Expression* e) const {
    return e->Is<ArrowFunctionExpression>() && e->end == prev_end();
  }

  template <typename T>
  T* New(int32_t start) {
    return tree_->New<T>(start, -1);
  }
  template <typename T>
  T* Finish(T* node) {
    node->end = prev_end();
    return node;
  }
  std::string_view Intern(const std::string& text) {
    return tree_->Intern(text);
  }
  std::string_view Slice(int32_t start, int32_t end) const {
    return source_.substr(static_cast<size_t>(start),
                          static_cast<size_t>(end - start));
  }

  // Identifier classification.
  bool IsIdentifierToken() const;  // kName usable as a binding/reference
  bool IsReservedWordInContext(std::string_view name) const;
  static bool TokenCanStartPropertyName(Tok type);
  bool IsLetDeclarationStart();
  bool IsUsingDeclarationStart();
  bool TokenStartsExpression() const;

  // --- program & statements ---
  Program* ParseProgram();
  bool ParseDirectivePrologue(std::vector<Statement*>* body, bool* strict);
  Statement* ParseStatementListItem();
  Statement* ParseStatement(bool single_statement);
  BlockStatement* ParseBlock();
  Statement* ParseVariableStatement(VariableKind kind, int32_t start);
  VariableDeclaration* ParseVariableDeclarationList(VariableKind kind,
                                                    int32_t start, bool no_in,
                                                    bool for_init);
  Statement* ParseIfStatement();
  Statement* ParseForStatement();
  Statement* ParseWhileStatement();
  Statement* ParseDoWhileStatement();
  Statement* ParseBreakContinue(bool is_break);
  Statement* ParseReturnStatement();
  Statement* ParseWithStatement();
  Statement* ParseSwitchStatement();
  Statement* ParseThrowStatement();
  Statement* ParseTryStatement();
  Statement* ParseExpressionOrLabeledStatement();
  Statement* ParseFunctionDeclaration(int32_t start, bool is_async);
  Statement* ParseClassDeclaration();

  // --- functions & classes ---
  bool ParseFunctionRest(FunctionData* fn, int32_t start, bool is_method,
                         bool is_getter, bool is_setter, bool is_constructor);
  bool ParseFormalParameters(FunctionData* fn);
  bool ParseFunctionBody(FunctionData* fn);
  Expression* ParseArrowFromParams(std::vector<Expression*> params,
                                   int32_t start, bool is_async);
  bool ParseArrowBody(ArrowFunctionExpression* arrow);
  bool ParseClass(ClassData* cls, int32_t start, bool is_expression);
  Node* ParseClassMember();
  Expression* ParsePropertyKey(bool* computed, bool allow_private);

  // --- expressions ---
  Expression* ParseExpression(bool no_in = false);
  Expression* ParseAssignment(bool no_in = false);
  Expression* ParseYield(bool no_in);
  Expression* ParseConditional(bool no_in);
  Expression* ParseBinary(int min_precedence, bool no_in);
  Expression* ParseUnary();
  Expression* ParsePostfix();
  Expression* ParseLeftHandSide();
  Expression* ParseSubscripts(Expression* base, int32_t start, bool no_calls);
  Expression* ParseNewExpression();
  Expression* ParsePrimary();
  Expression* ParseParenthesized();
  Expression* ParseArrayLiteral();
  Expression* ParseObjectLiteral();
  Node* ParseObjectProperty();
  Expression* ParseTemplateLiteral(bool tagged);
  Expression* ParseFunctionExpression(int32_t start, bool is_async);
  Expression* ParseNativeCall();
  Expression* ParseIdentifierReference();
  Identifier* ParseIdentifierName();  // any name incl. reserved words
  Identifier* ParseBindingIdentifier();
  Expression*
  ParseBindingPattern();  // Identifier / ObjectPattern / ArrayPattern
  Expression* ParseBindingElement();  // pattern with optional default
  bool ParseArguments(std::vector<Expression*>* arguments);
  Expression* ParseSpreadOrAssignment();
  Expression* ParseImportExpressionOrMeta();
  Expression* ParseSuper();

  // Cover grammar: reinterprets an expression as an assignment/binding
  // pattern. Returns nullptr (with an error) if that is not possible.
  Expression* ToPattern(Expression* expression, bool binding);
  bool IsValidSimpleAssignmentTarget(Expression* expression) const;

  Tree* tree_;
  std::string_view source_;
  ParseOptions options_;
  Lexer lexer_;
  int32_t prev_token_end_ = 0;
  bool failed_ = false;
  ParseError error_;
  FunctionState function_state_;
  std::vector<FunctionState> function_stack_;
  // Nesting depth guard against pathological inputs.
  int depth_ = 0;
};

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_PARSER_H_
