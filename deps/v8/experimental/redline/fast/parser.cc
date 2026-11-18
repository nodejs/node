// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/parser.h"

#include <algorithm>
#include <utility>

#include "experimental/redline/base/log.h"

namespace redline {
namespace fast {

namespace {

constexpr int kMaxDepth = 1200;

struct DepthGuard {
  explicit DepthGuard(int* depth) : depth_(depth) { (*depth_)++; }
  ~DepthGuard() { (*depth_)--; }
  int* depth_;
};

bool IsStrictReservedWord(std::string_view name) {
  return name == "let" || name == "static" || name == "implements" ||
         name == "interface" || name == "package" || name == "private" ||
         name == "protected" || name == "public";
}

int BinaryPrecedenceOfToken(Tok tok, bool no_in, BinaryOp* bin_op,
                            LogicalOp* log_op, bool* is_logical) {
  *is_logical = false;
  switch (tok) {
    case Tok::kNullish:
      *is_logical = true;
      *log_op = LogicalOp::kNullish;
      return 1;
    case Tok::kOr:
      *is_logical = true;
      *log_op = LogicalOp::kOr;
      return 2;
    case Tok::kAnd:
      *is_logical = true;
      *log_op = LogicalOp::kAnd;
      return 3;
    case Tok::kBitOr:
      *bin_op = BinaryOp::kBitOr;
      return 4;
    case Tok::kBitXor:
      *bin_op = BinaryOp::kBitXor;
      return 5;
    case Tok::kBitAnd:
      *bin_op = BinaryOp::kBitAnd;
      return 6;
    case Tok::kEq:
      *bin_op = BinaryOp::kEq;
      return 7;
    case Tok::kNotEq:
      *bin_op = BinaryOp::kNotEq;
      return 7;
    case Tok::kStrictEq:
      *bin_op = BinaryOp::kStrictEq;
      return 7;
    case Tok::kStrictNotEq:
      *bin_op = BinaryOp::kStrictNotEq;
      return 7;
    case Tok::kLessThan:
      *bin_op = BinaryOp::kLessThan;
      return 8;
    case Tok::kLessThanEq:
      *bin_op = BinaryOp::kLessThanEq;
      return 8;
    case Tok::kGreaterThan:
      *bin_op = BinaryOp::kGreaterThan;
      return 8;
    case Tok::kGreaterThanEq:
      *bin_op = BinaryOp::kGreaterThanEq;
      return 8;
    case Tok::kInstanceof:
      *bin_op = BinaryOp::kInstanceof;
      return 8;
    case Tok::kIn:
      if (no_in) {
        return 0;
      }
      *bin_op = BinaryOp::kIn;
      return 8;
    case Tok::kShl:
      *bin_op = BinaryOp::kShiftLeft;
      return 9;
    case Tok::kSar:
      *bin_op = BinaryOp::kShiftRight;
      return 9;
    case Tok::kShr:
      *bin_op = BinaryOp::kShiftRightLogical;
      return 9;
    case Tok::kAdd:
      *bin_op = BinaryOp::kAdd;
      return 10;
    case Tok::kSub:
      *bin_op = BinaryOp::kSub;
      return 10;
    case Tok::kMul:
      *bin_op = BinaryOp::kMul;
      return 11;
    case Tok::kDiv:
      *bin_op = BinaryOp::kDiv;
      return 11;
    case Tok::kMod:
      *bin_op = BinaryOp::kMod;
      return 11;
    case Tok::kExp:
      *bin_op = BinaryOp::kExp;
      return 12;
    default:
      return 0;
  }
}

bool AssignOpOfToken(Tok tok, AssignOp* op) {
  switch (tok) {
    case Tok::kAssign:
      *op = AssignOp::kAssign;
      return true;
    case Tok::kAssignAdd:
      *op = AssignOp::kAddAssign;
      return true;
    case Tok::kAssignSub:
      *op = AssignOp::kSubAssign;
      return true;
    case Tok::kAssignMul:
      *op = AssignOp::kMulAssign;
      return true;
    case Tok::kAssignDiv:
      *op = AssignOp::kDivAssign;
      return true;
    case Tok::kAssignMod:
      *op = AssignOp::kModAssign;
      return true;
    case Tok::kAssignExp:
      *op = AssignOp::kExpAssign;
      return true;
    case Tok::kAssignShl:
      *op = AssignOp::kShiftLeftAssign;
      return true;
    case Tok::kAssignSar:
      *op = AssignOp::kShiftRightAssign;
      return true;
    case Tok::kAssignShr:
      *op = AssignOp::kShiftRightLogicalAssign;
      return true;
    case Tok::kAssignBitAnd:
      *op = AssignOp::kBitAndAssign;
      return true;
    case Tok::kAssignBitOr:
      *op = AssignOp::kBitOrAssign;
      return true;
    case Tok::kAssignBitXor:
      *op = AssignOp::kBitXorAssign;
      return true;
    case Tok::kAssignAnd:
      *op = AssignOp::kAndAssign;
      return true;
    case Tok::kAssignOr:
      *op = AssignOp::kOrAssign;
      return true;
    case Tok::kAssignNullish:
      *op = AssignOp::kNullishAssign;
      return true;
    default:
      return false;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// ParseError

void ComputeLineColumn(std::string_view source, int32_t position, int* line,
                       int* column) {
  *line = 1;
  *column = 1;
  if (position < 0) {
    return;
  }
  size_t limit = std::min(static_cast<size_t>(position), source.size());
  for (size_t i = 0; i < limit; i++) {
    if (source[i] == '\n') {
      (*line)++;
      *column = 1;
    } else {
      (*column)++;
    }
  }
}

std::string ParseError::ToString() const {
  return StringPrintf("%d:%d: %s", line, column, message.c_str());
}

// ---------------------------------------------------------------------------
// Entry points

std::unique_ptr<Tree> Parser::ParseScript(std::string_view source,
                                          const ParseOptions& options,
                                          ParseError* error) {
  auto tree = std::make_unique<Tree>();
  tree->SetSource(source);
  Parser parser(tree.get(), tree->source(), options);
  Program* program = parser.ParseProgram();
  if (program == nullptr || parser.has_error()) {
    if (error != nullptr) {
      *error = parser.error_;
    }
    return nullptr;
  }
  tree->set_root(program);
  return tree;
}

Expression* Parser::ParseExpressionInto(Tree* tree, std::string_view source,
                                        const ParseOptions& options,
                                        ParseError* error) {
  std::string_view interned = tree->Intern(source);
  Parser parser(tree, interned, options);
  parser.Advance();
  Expression* expression = parser.ParseExpression();
  if (expression != nullptr && !parser.has_error() &&
      !parser.Check(Tok::kEOF)) {
    parser.FailUnexpected();
  }
  if (expression == nullptr || parser.has_error()) {
    if (error != nullptr) {
      *error = parser.error_;
    }
    return nullptr;
  }
  return expression;
}

Parser::Parser(Tree* tree, std::string_view source, const ParseOptions& options)
    : tree_(tree), source_(source), options_(options), lexer_(source) {
  function_state_.strict = options.strict;
}

// ---------------------------------------------------------------------------
// Token helpers

void Parser::Advance() {
  prev_token_end_ = cur().end;
  lexer_.Next();
  if (lexer_.has_error() && !failed_) {
    Fail(lexer_.error(), lexer_.error_pos());
  }
}

bool Parser::Eat(Tok type) {
  if (!Check(type)) {
    return false;
  }
  Advance();
  return true;
}

bool Parser::EatName(std::string_view name) {
  if (!CheckName(name)) {
    return false;
  }
  Advance();
  return true;
}

bool Parser::Expect(Tok type) {
  if (Check(type)) {
    Advance();
    return true;
  }
  return FailUnexpected();
}

bool Parser::ExpectName(std::string_view name) {
  if (CheckName(name)) {
    Advance();
    return true;
  }
  return FailUnexpected();
}

bool Parser::CanInsertSemicolon() const {
  return Check(Tok::kEOF) || Check(Tok::kRightBrace) || cur().newline_before;
}

bool Parser::ConsumeSemicolon() {
  if (Eat(Tok::kSemicolon)) {
    return true;
  }
  if (CanInsertSemicolon()) {
    return true;
  }
  return FailUnexpected();
}

Token Parser::PeekNext() {
  Lexer::State state = lexer_.Save();
  Token next = lexer_.Next();
  lexer_.Restore(state);
  return next;
}

Token Parser::PeekNext2() {
  Lexer::State state = lexer_.Save();
  lexer_.Next();
  Token next2 = lexer_.Next();
  lexer_.Restore(state);
  return next2;
}

bool Parser::Fail(const std::string& message, int32_t position) {
  if (!failed_) {
    failed_ = true;
    error_.message = message;
    error_.position = position;
    ComputeLineColumn(source_, position, &error_.line, &error_.column);
  }
  return false;
}

bool Parser::FailUnexpected() {
  if (failed_) {
    return false;
  }
  const Token& token = cur();
  std::string message;
  switch (token.type) {
    case Tok::kEOF:
      message = "Unexpected end of input";
      break;
    case Tok::kNumber:
    case Tok::kBigInt:
      message = "Unexpected number";
      break;
    case Tok::kString:
      message = "Unexpected string";
      break;
    case Tok::kTemplateHead:
    case Tok::kNoSubstitutionTemplate:
    case Tok::kTemplateMiddle:
    case Tok::kTemplateTail:
      message = "Unexpected template string";
      break;
    case Tok::kName:
      message = IsReservedWordInContext(token.value)
                    ? "Unexpected reserved word"
                    : "Unexpected identifier '" + token.value + "'";
      break;
    case Tok::kIllegal:
      message = "Invalid or unexpected token";
      break;
    default:
      message = std::string("Unexpected token '") +
                std::string(lexer_.TokenText(token)) + "'";
      break;
  }
  return Fail(message, token.start);
}

// ---------------------------------------------------------------------------
// Identifier classification

bool Parser::IsReservedWordInContext(std::string_view name) const {
  if (name == "yield") {
    return function_state_.strict || function_state_.is_generator;
  }
  if (name == "await") {
    return function_state_.is_async || function_state_.in_static_block;
  }
  if (function_state_.strict && IsStrictReservedWord(name)) {
    return true;
  }
  return false;
}

bool Parser::IsIdentifierToken() const {
  if (!Check(Tok::kName)) {
    return false;
  }
  if (cur().has_escape) {
    return true;  // lenient
  }
  return !IsReservedWordInContext(cur().value);
}

bool Parser::TokenCanStartPropertyName(Tok type) {
  switch (type) {
    case Tok::kName:
    case Tok::kPrivateName:
    case Tok::kString:
    case Tok::kNumber:
    case Tok::kBigInt:
    case Tok::kLeftBracket:
      return true;
    default:
      return IsKeyword(type);
  }
}

bool Parser::IsLetDeclarationStart() {
  // Current token is the name `let`.
  if (function_state_.strict) {
    return true;
  }
  Token next = PeekNext();
  if (next.type == Tok::kLeftBracket || next.type == Tok::kLeftBrace) {
    return true;
  }
  if (next.type == Tok::kName) {
    // `let` followed by an identifier (on any line) is a declaration; `let`
    // followed by a keyword such as `in` or `instanceof` is an identifier.
    return true;
  }
  return false;
}

bool Parser::IsUsingDeclarationStart() {
  // Current token is the name `using`; a declaration if followed on the
  // same line by a binding identifier that is not `in`/`of`.
  Token next = PeekNext();
  if (next.newline_before) {
    return false;
  }
  if (next.type != Tok::kName) {
    return false;
  }
  if (next.value == "of" && !next.has_escape) {
    // `for (using of x)` iterates with the identifier `using`, but V8 (and
    // the spec) accept `using of = 1` as a declaration of `of` in C-style
    // for loops and plain statements, and `for (using of of x)` declares `of`.
    Token next2 = PeekNext2();
    return next2.type == Tok::kAssign ||
           (next2.type == Tok::kName && !next2.has_escape &&
            next2.value == "of");
  }
  return true;
}

bool Parser::TokenStartsExpression() const {
  switch (peek_type()) {
    case Tok::kName:
    case Tok::kPrivateName:
    case Tok::kNumber:
    case Tok::kBigInt:
    case Tok::kString:
    case Tok::kTemplateHead:
    case Tok::kNoSubstitutionTemplate:
    case Tok::kRegExp:
    case Tok::kLeftParen:
    case Tok::kLeftBracket:
    case Tok::kLeftBrace:
    case Tok::kAdd:
    case Tok::kSub:
    case Tok::kNot:
    case Tok::kBitNot:
    case Tok::kInc:
    case Tok::kDec:
    case Tok::kDiv:
    case Tok::kAssignDiv:
    case Tok::kThis:
    case Tok::kSuper:
    case Tok::kNew:
    case Tok::kFunction:
    case Tok::kClass:
    case Tok::kTypeof:
    case Tok::kVoid:
    case Tok::kDelete:
    case Tok::kNull:
    case Tok::kTrue:
    case Tok::kFalse:
    case Tok::kImport:
    case Tok::kMod:
      return true;
    default:
      return false;
  }
}

// ---------------------------------------------------------------------------
// Program and statements

Program* Parser::ParseProgram() {
  Program* program = New<Program>(0);
  Advance();
  if (has_error()) {
    return nullptr;
  }
  bool strict = function_state_.strict;
  if (!ParseDirectivePrologue(&program->body, &strict)) {
    return nullptr;
  }
  program->strict = strict;
  while (!Check(Tok::kEOF)) {
    if (has_error()) {
      return nullptr;
    }
    Statement* statement = ParseStatementListItem();
    if (statement == nullptr) {
      return nullptr;
    }
    program->body.push_back(statement);
  }
  program->end = static_cast<int32_t>(source_.size());
  return program;
}

// Parses leading string-literal expression statements. Sets *strict when a
// "use strict" directive is found (and switches the parser to strict mode).
bool Parser::ParseDirectivePrologue(std::vector<Statement*>* body,
                                    bool* strict) {
  while (Check(Tok::kString)) {
    Token string_token = cur();
    int32_t start = start_pos();
    Statement* statement = ParseStatementListItem();
    if (statement == nullptr) {
      return false;
    }
    body->push_back(statement);
    auto* expression_statement = statement->As<ExpressionStatement>();
    if (expression_statement == nullptr) {
      return true;
    }
    auto* literal = expression_statement->expression->As<StringLiteral>();
    if (literal == nullptr || literal->start != start ||
        literal->end != string_token.end) {
      return true;  // e.g. "a" + b; or "a".length; -- not a directive
    }
    expression_statement->is_directive = true;
    expression_statement->directive =
        Slice(string_token.start + 1, string_token.end - 1);
    if (expression_statement->directive == "use strict") {
      *strict = true;
      function_state_.strict = true;
    }
  }
  return true;
}

Statement* Parser::ParseStatementListItem() { return ParseStatement(false); }

// `single_statement` is true for the body of if/loops/with/labels, where
// declarations are not allowed (`if (x) let y;` is `let` the identifier
// followed by an error, `if (x) const y = 1;` is a SyntaxError). Annex B
// allows plain function declarations there in sloppy mode.
Statement* Parser::ParseStatement(bool single_statement) {
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return nullptr;
  }
  if (has_error()) {
    return nullptr;
  }
  int32_t start = start_pos();
  switch (peek_type()) {
    case Tok::kLeftBrace:
      return ParseBlock();
    case Tok::kSemicolon: {
      Advance();
      return Finish(New<EmptyStatement>(start));
    }
    case Tok::kVar:
      return ParseVariableStatement(VariableKind::kVar, start);
    case Tok::kConst:
      if (single_statement) {
        Fail("Lexical declaration cannot appear in a single-statement context",
             start);
        return nullptr;
      }
      return ParseVariableStatement(VariableKind::kConst, start);
    case Tok::kIf:
      return ParseIfStatement();
    case Tok::kFor:
      return ParseForStatement();
    case Tok::kWhile:
      return ParseWhileStatement();
    case Tok::kDo:
      return ParseDoWhileStatement();
    case Tok::kContinue:
      return ParseBreakContinue(false);
    case Tok::kBreak:
      return ParseBreakContinue(true);
    case Tok::kReturn:
      return ParseReturnStatement();
    case Tok::kWith:
      return ParseWithStatement();
    case Tok::kSwitch:
      return ParseSwitchStatement();
    case Tok::kThrow:
      return ParseThrowStatement();
    case Tok::kTry:
      return ParseTryStatement();
    case Tok::kDebugger: {
      Advance();
      if (!ConsumeSemicolon()) {
        return nullptr;
      }
      return Finish(New<DebuggerStatement>(start));
    }
    case Tok::kFunction:
      if (single_statement &&
          (function_state_.strict || PeekNext().type == Tok::kMul)) {
        Fail(
            "Function declarations are not allowed in a single-statement "
            "context",
            start);
        return nullptr;
      }
      return ParseFunctionDeclaration(start, false);
    case Tok::kClass:
      if (single_statement) {
        Fail("Class declaration cannot appear in a single-statement context",
             start);
        return nullptr;
      }
      return ParseClassDeclaration();
    case Tok::kImport: {
      Token next = PeekNext();
      if (next.type == Tok::kLeftParen || next.type == Tok::kPeriod) {
        return ParseExpressionOrLabeledStatement();
      }
      Fail("Cannot use import statement outside a module", start);
      return nullptr;
    }
    case Tok::kExport:
      Fail("Unexpected token 'export'", start);
      return nullptr;
    case Tok::kName: {
      if (cur().has_escape) {
        return ParseExpressionOrLabeledStatement();
      }
      const std::string& name = cur().value;
      if (name == "let") {
        if (single_statement) {
          // ExpressionStatement has a [lookahead != `let [`] restriction;
          // any other `let` here is an identifier (and `let x` is an error
          // reported when the `x` is not followed by a terminator).
          if (PeekNext().type == Tok::kLeftBracket) {
            Fail(
                "Lexical declaration cannot appear in a single-statement "
                "context",
                start);
            return nullptr;
          }
        } else if (IsLetDeclarationStart()) {
          return ParseVariableStatement(VariableKind::kLet, start);
        }
      }
      if (name == "async") {
        Token next = PeekNext();
        if (next.type == Tok::kFunction && !next.newline_before) {
          if (single_statement) {
            Fail(
                "Async functions can only be declared at the top level or "
                "inside a block",
                start);
            return nullptr;
          }
          Advance();
          return ParseFunctionDeclaration(start, true);
        }
      }
      if (name == "using" && !single_statement && IsUsingDeclarationStart()) {
        return ParseVariableStatement(VariableKind::kUsing, start);
      }
      if (name == "await" && !single_statement && function_state_.is_async) {
        Token next = PeekNext();
        if (next.type == Tok::kName && !next.has_escape &&
            next.value == "using" && !next.newline_before) {
          Token next2 = PeekNext2();
          if (next2.type == Tok::kName && !next2.newline_before) {
            Advance();  // await
            return ParseVariableStatement(VariableKind::kAwaitUsing, start);
          }
        }
      }
      return ParseExpressionOrLabeledStatement();
    }
    default:
      return ParseExpressionOrLabeledStatement();
  }
}

BlockStatement* Parser::ParseBlock() {
  BlockStatement* block = New<BlockStatement>(start_pos());
  if (!Expect(Tok::kLeftBrace)) {
    return nullptr;
  }
  while (!Check(Tok::kRightBrace)) {
    if (has_error() || Check(Tok::kEOF)) {
      FailUnexpected();
      return nullptr;
    }
    Statement* statement = ParseStatementListItem();
    if (statement == nullptr) {
      return nullptr;
    }
    block->body.push_back(statement);
  }
  Advance();
  return Finish(block);
}

Statement* Parser::ParseVariableStatement(VariableKind kind, int32_t start) {
  Advance();  // var / let / const / using
  VariableDeclaration* declaration =
      ParseVariableDeclarationList(kind, start, false, false);
  if (declaration == nullptr) {
    return nullptr;
  }
  if (!ConsumeSemicolon()) {
    return nullptr;
  }
  return Finish(declaration);
}

VariableDeclaration* Parser::ParseVariableDeclarationList(VariableKind kind,
                                                          int32_t start,
                                                          bool no_in,
                                                          bool for_init) {
  VariableDeclaration* declaration = New<VariableDeclaration>(start);
  declaration->declaration_kind = kind;
  while (true) {
    VariableDeclarator* declarator = New<VariableDeclarator>(start_pos());
    declarator->target = ParseBindingPattern();
    if (declarator->target == nullptr) {
      return nullptr;
    }
    if (Eat(Tok::kAssign)) {
      declarator->init = ParseAssignment(no_in);
      if (declarator->init == nullptr) {
        return nullptr;
      }
    } else if (!for_init && (kind == VariableKind::kConst ||
                             !declarator->target->Is<Identifier>())) {
      // `const x;` and `let {a};` are errors; the target rejects them too,
      // but a missing initializer makes the tree lie about the semantics.
      Fail("Missing initializer in declaration", prev_end());
      return nullptr;
    }
    Finish(declarator);
    declaration->declarations.push_back(declarator);
    if (!Eat(Tok::kComma)) {
      break;
    }
  }
  return Finish(declaration);
}

Statement* Parser::ParseIfStatement() {
  IfStatement* statement = New<IfStatement>(start_pos());
  Advance();
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }
  statement->test = ParseExpression();
  if (statement->test == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  statement->consequent = ParseStatement(true);
  if (statement->consequent == nullptr) {
    return nullptr;
  }
  if (Eat(Tok::kElse)) {
    statement->alternate = ParseStatement(true);
    if (statement->alternate == nullptr) {
      return nullptr;
    }
  }
  return Finish(statement);
}

Statement* Parser::ParseForStatement() {
  int32_t start = start_pos();
  Advance();  // for
  bool is_await = false;
  if (CheckName("await")) {
    is_await = true;
    Advance();
  }
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }

  Node* init = nullptr;
  Node* left = nullptr;
  bool is_declaration = false;
  VariableKind kind = VariableKind::kVar;
  if (Check(Tok::kVar)) {
    is_declaration = true;
    kind = VariableKind::kVar;
  } else if (Check(Tok::kConst)) {
    is_declaration = true;
    kind = VariableKind::kConst;
  } else if (CheckName("let") && IsLetDeclarationStart()) {
    is_declaration = true;
    kind = VariableKind::kLet;
  } else if (CheckName("using") && IsUsingDeclarationStart()) {
    is_declaration = true;
    kind = VariableKind::kUsing;
  } else if (CheckName("await") && function_state_.is_async) {
    Token next = PeekNext();
    if (next.type == Tok::kName && !next.has_escape && next.value == "using" &&
        !next.newline_before) {
      Token next2 = PeekNext2();
      if (next2.type == Tok::kName && !next2.newline_before) {
        Advance();
        is_declaration = true;
        kind = VariableKind::kAwaitUsing;
      }
    }
  }

  if (Check(Tok::kSemicolon)) {
    // for (;;)
  } else if (is_declaration) {
    int32_t declaration_start = start_pos();
    Advance();  // var / let / const / using
    VariableDeclaration* declaration =
        ParseVariableDeclarationList(kind, declaration_start, true, true);
    if (declaration == nullptr) {
      return nullptr;
    }
    if (Check(Tok::kIn) || CheckName("of")) {
      left = declaration;
    } else {
      init = declaration;
    }
  } else {
    Expression* expression = ParseExpression(true);
    if (expression == nullptr) {
      return nullptr;
    }
    if (Check(Tok::kIn) || CheckName("of")) {
      if (expression->Is<ObjectExpression>() ||
          expression->Is<ArrayExpression>()) {
        expression = ToPattern(expression, false);
        if (expression == nullptr) {
          return nullptr;
        }
      } else if (!IsValidSimpleAssignmentTarget(expression)) {
        Fail("Invalid left-hand side in for-loop", expression->start);
        return nullptr;
      }
      left = expression;
    } else {
      init = expression;
    }
  }

  if (left != nullptr) {
    bool is_of = CheckName("of");
    Advance();  // in / of
    Expression* right = is_of ? ParseAssignment() : ParseExpression();
    if (right == nullptr) {
      return nullptr;
    }
    if (!Expect(Tok::kRightParen)) {
      return nullptr;
    }
    Statement* body = ParseStatement(true);
    if (body == nullptr) {
      return nullptr;
    }
    if (is_of) {
      ForOfStatement* statement = New<ForOfStatement>(start);
      statement->left = left;
      statement->right = right;
      statement->body = body;
      statement->is_await = is_await;
      return Finish(statement);
    }
    if (is_await) {
      Fail("Unexpected token 'in'", start);
      return nullptr;
    }
    ForInStatement* statement = New<ForInStatement>(start);
    statement->left = left;
    statement->right = right;
    statement->body = body;
    return Finish(statement);
  }

  if (is_await) {
    FailUnexpected();
    return nullptr;
  }
  ForStatement* statement = New<ForStatement>(start);
  statement->init = init;
  if (!Expect(Tok::kSemicolon)) {
    return nullptr;
  }
  if (!Check(Tok::kSemicolon)) {
    statement->test = ParseExpression();
    if (statement->test == nullptr) {
      return nullptr;
    }
  }
  if (!Expect(Tok::kSemicolon)) {
    return nullptr;
  }
  if (!Check(Tok::kRightParen)) {
    statement->update = ParseExpression();
    if (statement->update == nullptr) {
      return nullptr;
    }
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  statement->body = ParseStatement(true);
  if (statement->body == nullptr) {
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseWhileStatement() {
  WhileStatement* statement = New<WhileStatement>(start_pos());
  Advance();
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }
  statement->test = ParseExpression();
  if (statement->test == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  statement->body = ParseStatement(true);
  if (statement->body == nullptr) {
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseDoWhileStatement() {
  DoWhileStatement* statement = New<DoWhileStatement>(start_pos());
  Advance();
  statement->body = ParseStatement(true);
  if (statement->body == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kWhile)) {
    return nullptr;
  }
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }
  statement->test = ParseExpression();
  if (statement->test == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  // A semicolon after do-while is optional even without a newline.
  Eat(Tok::kSemicolon);
  return Finish(statement);
}

Statement* Parser::ParseBreakContinue(bool is_break) {
  int32_t start = start_pos();
  Advance();
  Identifier* label = nullptr;
  if (Check(Tok::kName) && !cur().newline_before && IsIdentifierToken()) {
    label = ParseBindingIdentifier();
    if (label == nullptr) {
      return nullptr;
    }
  }
  if (!ConsumeSemicolon()) {
    return nullptr;
  }
  if (is_break) {
    BreakStatement* statement = New<BreakStatement>(start);
    statement->label = label;
    return Finish(statement);
  }
  ContinueStatement* statement = New<ContinueStatement>(start);
  statement->label = label;
  return Finish(statement);
}

Statement* Parser::ParseReturnStatement() {
  ReturnStatement* statement = New<ReturnStatement>(start_pos());
  Advance();
  if (!Check(Tok::kSemicolon) && !CanInsertSemicolon()) {
    statement->argument = ParseExpression();
    if (statement->argument == nullptr) {
      return nullptr;
    }
  }
  if (!ConsumeSemicolon()) {
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseWithStatement() {
  WithStatement* statement = New<WithStatement>(start_pos());
  Advance();
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }
  statement->object = ParseExpression();
  if (statement->object == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  statement->body = ParseStatement(true);
  if (statement->body == nullptr) {
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseSwitchStatement() {
  SwitchStatement* statement = New<SwitchStatement>(start_pos());
  Advance();
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }
  statement->discriminant = ParseExpression();
  if (statement->discriminant == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  if (!Expect(Tok::kLeftBrace)) {
    return nullptr;
  }
  while (!Check(Tok::kRightBrace)) {
    if (has_error()) {
      return nullptr;
    }
    SwitchCase* switch_case = New<SwitchCase>(start_pos());
    if (Eat(Tok::kCase)) {
      switch_case->test = ParseExpression();
      if (switch_case->test == nullptr) {
        return nullptr;
      }
    } else if (!Expect(Tok::kDefault)) {
      return nullptr;
    }
    if (!Expect(Tok::kColon)) {
      return nullptr;
    }
    while (!Check(Tok::kCase) && !Check(Tok::kDefault) &&
           !Check(Tok::kRightBrace)) {
      if (has_error() || Check(Tok::kEOF)) {
        FailUnexpected();
        return nullptr;
      }
      Statement* consequent = ParseStatementListItem();
      if (consequent == nullptr) {
        return nullptr;
      }
      switch_case->consequent.push_back(consequent);
    }
    statement->cases.push_back(Finish(switch_case));
  }
  Advance();
  return Finish(statement);
}

Statement* Parser::ParseThrowStatement() {
  ThrowStatement* statement = New<ThrowStatement>(start_pos());
  Advance();
  if (cur().newline_before) {
    Fail("Illegal newline after throw", start_pos());
    return nullptr;
  }
  statement->argument = ParseExpression();
  if (statement->argument == nullptr) {
    return nullptr;
  }
  if (!ConsumeSemicolon()) {
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseTryStatement() {
  TryStatement* statement = New<TryStatement>(start_pos());
  Advance();
  statement->block = ParseBlock();
  if (statement->block == nullptr) {
    return nullptr;
  }
  if (Check(Tok::kCatch)) {
    CatchClause* handler = New<CatchClause>(start_pos());
    Advance();
    if (Eat(Tok::kLeftParen)) {
      handler->param = ParseBindingPattern();
      if (handler->param == nullptr) {
        return nullptr;
      }
      if (!Expect(Tok::kRightParen)) {
        return nullptr;
      }
    }
    handler->body = ParseBlock();
    if (handler->body == nullptr) {
      return nullptr;
    }
    statement->handler = Finish(handler);
  }
  if (Eat(Tok::kFinally)) {
    statement->finalizer = ParseBlock();
    if (statement->finalizer == nullptr) {
      return nullptr;
    }
  }
  if (statement->handler == nullptr && statement->finalizer == nullptr) {
    Fail("Missing catch or finally after try", start_pos());
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseExpressionOrLabeledStatement() {
  int32_t start = start_pos();
  if (Check(Tok::kName) && IsIdentifierToken()) {
    Token next = PeekNext();
    if (next.type == Tok::kColon) {
      LabeledStatement* statement = New<LabeledStatement>(start);
      statement->label = ParseBindingIdentifier();
      if (statement->label == nullptr) {
        return nullptr;
      }
      Advance();  // ':'
      statement->body = ParseStatement(true);
      if (statement->body == nullptr) {
        return nullptr;
      }
      return Finish(statement);
    }
  }
  ExpressionStatement* statement = New<ExpressionStatement>(start);
  statement->expression = ParseExpression();
  if (statement->expression == nullptr) {
    return nullptr;
  }
  if (!ConsumeSemicolon()) {
    return nullptr;
  }
  return Finish(statement);
}

Statement* Parser::ParseFunctionDeclaration(int32_t start, bool is_async) {
  FunctionDeclaration* fn = New<FunctionDeclaration>(start);
  fn->is_async = is_async;
  Advance();  // function
  fn->is_generator = Eat(Tok::kMul);
  fn->name = ParseBindingIdentifier();
  if (fn->name == nullptr) {
    return nullptr;
  }
  if (!ParseFunctionRest(fn, start, false, false, false, false)) {
    return nullptr;
  }
  return Finish(fn);
}

Statement* Parser::ParseClassDeclaration() {
  ClassDeclaration* cls = New<ClassDeclaration>(start_pos());
  if (!ParseClass(cls, cls->start, false)) {
    return nullptr;
  }
  if (cls->name == nullptr) {
    Fail("Class declarations require a name", cls->start);
    return nullptr;
  }
  return Finish(cls);
}

// ---------------------------------------------------------------------------
// Functions and classes

bool Parser::ParseFunctionRest(FunctionData* fn, int32_t /*start*/,
                               bool /*is_method*/, bool /*is_getter*/,
                               bool /*is_setter*/, bool /*is_constructor*/
) {
  function_stack_.push_back(function_state_);
  FunctionState state;
  state.is_async = fn->is_async;
  state.is_generator = fn->is_generator;
  state.is_arrow = false;
  state.in_function = true;
  state.strict = function_state_.strict;
  function_state_ = state;
  bool ok = ParseFormalParameters(fn) && ParseFunctionBody(fn);
  fn->strict = function_state_.strict;
  function_state_ = function_stack_.back();
  function_stack_.pop_back();
  return ok;
}

bool Parser::ParseFormalParameters(FunctionData* fn) {
  if (!Expect(Tok::kLeftParen)) {
    return false;
  }
  while (!Check(Tok::kRightParen)) {
    if (has_error()) {
      return false;
    }
    if (Check(Tok::kEllipsis)) {
      RestElement* rest = New<RestElement>(start_pos());
      Advance();
      rest->argument = ParseBindingPattern();
      if (rest->argument == nullptr) {
        return false;
      }
      fn->params.push_back(Finish(rest));
      if (!Check(Tok::kRightParen)) {
        return FailUnexpected();
      }
      break;
    }
    Expression* param = ParseBindingElement();
    if (param == nullptr) {
      return false;
    }
    fn->params.push_back(param);
    if (!Check(Tok::kRightParen) && !Expect(Tok::kComma)) {
      return false;
    }
  }
  return Expect(Tok::kRightParen);
}

bool Parser::ParseFunctionBody(FunctionData* fn) {
  BlockStatement* body = New<BlockStatement>(start_pos());
  if (!Expect(Tok::kLeftBrace)) {
    return false;
  }
  bool strict = function_state_.strict;
  if (!ParseDirectivePrologue(&body->body, &strict)) {
    return false;
  }
  while (!Check(Tok::kRightBrace)) {
    if (has_error() || Check(Tok::kEOF)) {
      return FailUnexpected();
    }
    Statement* statement = ParseStatementListItem();
    if (statement == nullptr) {
      return false;
    }
    body->body.push_back(statement);
  }
  Advance();
  fn->body = Finish(body);
  return true;
}

bool Parser::ParseArrowBody(ArrowFunctionExpression* arrow) {
  if (Check(Tok::kLeftBrace)) {
    return ParseFunctionBody(arrow);
  }
  arrow->expression_body = ParseAssignment();
  return arrow->expression_body != nullptr;
}

Expression* Parser::ParseArrowFromParams(std::vector<Expression*> params,
                                         int32_t start, bool is_async) {
  ArrowFunctionExpression* arrow = New<ArrowFunctionExpression>(start);
  arrow->is_arrow = true;
  arrow->is_async = is_async;
  for (size_t i = 0; i < params.size(); i++) {
    Expression* param = params[i];
    if (auto* spread = param->As<SpreadElement>()) {
      if (i + 1 != params.size()) {
        Fail("Rest parameter must be last formal parameter", spread->start);
        return nullptr;
      }
      RestElement* rest = New<RestElement>(spread->start);
      rest->argument = ToPattern(spread->argument, true);
      if (rest->argument == nullptr) {
        return nullptr;
      }
      rest->end = spread->end;
      param = rest;
    } else {
      param = ToPattern(param, true);
      if (param == nullptr) {
        return nullptr;
      }
    }
    arrow->params.push_back(param);
  }
  function_stack_.push_back(function_state_);
  FunctionState state;
  state.is_async = is_async;
  state.is_generator = false;
  state.is_arrow = true;
  state.in_function = true;
  state.strict = function_state_.strict;
  function_state_ = state;
  bool ok = ParseArrowBody(arrow);
  arrow->strict = function_state_.strict;
  function_state_ = function_stack_.back();
  function_stack_.pop_back();
  if (!ok) {
    return nullptr;
  }
  return Finish(arrow);
}

bool Parser::ParseClass(ClassData* cls, int32_t /*start*/,
                        bool /*is_expression*/) {
  // A class expression in the heritage clause recurses into ParseClass
  // through ParseLeftHandSide and ParsePrimary, which have no guard.
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return false;
  }
  Advance();  // class
  bool saved_strict = function_state_.strict;
  function_state_.strict = true;
  if (Check(Tok::kName) && !CheckName("extends")) {
    cls->name = ParseBindingIdentifier();
    if (cls->name == nullptr) {
      return false;
    }
  }
  if (Eat(Tok::kExtends)) {
    cls->super_class = ParseLeftHandSide();
    if (cls->super_class == nullptr) {
      return false;
    }
  }
  ClassBody* body = New<ClassBody>(start_pos());
  if (!Expect(Tok::kLeftBrace)) {
    return false;
  }
  while (!Check(Tok::kRightBrace)) {
    if (has_error() || Check(Tok::kEOF)) {
      return FailUnexpected();
    }
    if (Eat(Tok::kSemicolon)) {
      continue;
    }
    Node* member = ParseClassMember();
    if (member == nullptr) {
      return false;
    }
    body->members.push_back(member);
  }
  Advance();
  cls->body = Finish(body);
  function_state_.strict = saved_strict;
  return true;
}

Node* Parser::ParseClassMember() {
  int32_t start = start_pos();
  bool is_static = false;
  if (CheckName("static")) {
    Token next = PeekNext();
    if (next.type == Tok::kLeftBrace) {
      // static { ... }
      Advance();
      StaticBlock* block = New<StaticBlock>(start);
      Advance();  // '{'
      function_stack_.push_back(function_state_);
      FunctionState state;
      state.in_function = true;
      state.in_static_block = true;
      state.strict = true;
      function_state_ = state;
      bool ok = true;
      while (!Check(Tok::kRightBrace)) {
        if (has_error() || Check(Tok::kEOF)) {
          ok = FailUnexpected();
          break;
        }
        Statement* statement = ParseStatementListItem();
        if (statement == nullptr) {
          ok = false;
          break;
        }
        block->body.push_back(statement);
      }
      function_state_ = function_stack_.back();
      function_stack_.pop_back();
      if (!ok) {
        return nullptr;
      }
      Advance();  // '}'
      return Finish(block);
    }
    bool is_modifier =
        !(next.type == Tok::kLeftParen || next.type == Tok::kAssign ||
          next.type == Tok::kSemicolon || next.type == Tok::kRightBrace);
    if (is_modifier) {
      is_static = true;
      Advance();
    }
  }

  MethodKind method_kind = MethodKind::kMethod;
  bool is_async = false;
  bool is_generator = false;
  bool is_accessor = false;
  if (CheckName("get") || CheckName("set") || CheckName("async")) {
    Token next = PeekNext();
    bool is_modifier =
        !(next.type == Tok::kLeftParen || next.type == Tok::kAssign ||
          next.type == Tok::kSemicolon || next.type == Tok::kRightBrace ||
          (cur().value == "async" && next.newline_before));
    // `get`/`set` must be followed by a property name; `get\n*m() {}` is a
    // field named `get` (ASI) followed by a generator method.
    if (is_modifier && cur().value != "async" &&
        !TokenCanStartPropertyName(next.type)) {
      is_modifier = false;
    }
    if (is_modifier) {
      if (cur().value == "get") {
        method_kind = MethodKind::kGet;
        is_accessor = true;
      } else if (cur().value == "set") {
        method_kind = MethodKind::kSet;
        is_accessor = true;
      } else {
        is_async = true;
      }
      Advance();
    }
  }
  if (Check(Tok::kMul)) {
    is_generator = true;
    Advance();
  }
  // `accessor` [no LineTerminator here] ClassElementName: an auto-accessor
  // field. Otherwise `accessor` is an ordinary field or method name.
  bool is_auto_accessor = false;
  if (!is_accessor && !is_async && !is_generator && CheckName("accessor")) {
    Token next = PeekNext();
    bool is_modifier =
        !(next.type == Tok::kLeftParen || next.type == Tok::kAssign ||
          next.type == Tok::kSemicolon || next.type == Tok::kRightBrace ||
          next.newline_before);
    if (is_modifier) {
      is_auto_accessor = true;
      Advance();
    }
  }

  bool computed = false;
  int32_t key_start = start_pos();
  Expression* key = ParsePropertyKey(&computed, true);
  if (key == nullptr) {
    return nullptr;
  }

  if (!is_auto_accessor &&
      (Check(Tok::kLeftParen) || is_accessor || is_async || is_generator)) {
    MethodDefinition* method = New<MethodDefinition>(start);
    method->key = key;
    method->computed = computed;
    method->is_static = is_static;
    method->method_kind = method_kind;
    if (!is_static && !computed && method_kind == MethodKind::kMethod) {
      bool is_constructor = false;
      if (auto* id = key->As<Identifier>()) {
        is_constructor = id->name == "constructor";
      } else if (auto* str = key->As<StringLiteral>()) {
        is_constructor = str->value == "constructor";
      }
      if (is_constructor) {
        method->method_kind = MethodKind::kConstructor;
      }
    }
    FunctionExpression* fn = New<FunctionExpression>(key_start);
    fn->is_async = is_async;
    fn->is_generator = is_generator;
    if (!ParseFunctionRest(fn, key_start, true, method_kind == MethodKind::kGet,
                           method_kind == MethodKind::kSet,
                           method->method_kind == MethodKind::kConstructor)) {
      return nullptr;
    }
    method->value = Finish(fn);
    return Finish(method);
  }

  // Field definition.
  PropertyDefinition* field = New<PropertyDefinition>(start);
  field->key = key;
  field->computed = computed;
  field->is_static = is_static;
  field->is_auto_accessor = is_auto_accessor;
  if (Eat(Tok::kAssign)) {
    function_stack_.push_back(function_state_);
    FunctionState state;
    state.in_function = true;
    state.in_class_field_init = true;
    state.strict = true;
    function_state_ = state;
    field->value = ParseAssignment();
    function_state_ = function_stack_.back();
    function_stack_.pop_back();
    if (field->value == nullptr) {
      return nullptr;
    }
  }
  if (!ConsumeSemicolon()) {
    return nullptr;
  }
  return Finish(field);
}

Expression* Parser::ParsePropertyKey(bool* computed, bool allow_private) {
  *computed = false;
  int32_t start = start_pos();
  switch (peek_type()) {
    case Tok::kString: {
      StringLiteral* literal = New<StringLiteral>(start);
      literal->value = cur().value;
      literal->raw = Slice(cur().start, cur().end);
      Advance();
      return Finish(literal);
    }
    case Tok::kNumber: {
      NumericLiteral* literal = New<NumericLiteral>(start);
      literal->value = cur().number_value;
      literal->raw = Slice(cur().start, cur().end);
      Advance();
      return Finish(literal);
    }
    case Tok::kBigInt: {
      BigIntLiteral* literal = New<BigIntLiteral>(start);
      literal->raw = Slice(cur().start, cur().end);
      Advance();
      return Finish(literal);
    }
    case Tok::kLeftBracket: {
      Advance();
      *computed = true;
      Expression* key = ParseAssignment();
      if (key == nullptr) {
        return nullptr;
      }
      if (!Expect(Tok::kRightBracket)) {
        return nullptr;
      }
      return key;
    }
    case Tok::kPrivateName: {
      if (!allow_private) {
        FailUnexpected();
        return nullptr;
      }
      PrivateIdentifier* id = New<PrivateIdentifier>(start);
      id->name = Intern(cur().value);
      Advance();
      return Finish(id);
    }
    default:
      if (Check(Tok::kName) || IsKeyword(peek_type())) {
        return ParseIdentifierName();
      }
      FailUnexpected();
      return nullptr;
  }
}

// ---------------------------------------------------------------------------
// Expressions

Expression* Parser::ParseExpression(bool no_in) {
  int32_t start = start_pos();
  Expression* expression = ParseAssignment(no_in);
  if (expression == nullptr) {
    return nullptr;
  }
  if (!Check(Tok::kComma)) {
    return expression;
  }
  SequenceExpression* sequence = New<SequenceExpression>(start);
  sequence->expressions.push_back(expression);
  while (Eat(Tok::kComma)) {
    Expression* next = ParseAssignment(no_in);
    if (next == nullptr) {
      return nullptr;
    }
    sequence->expressions.push_back(next);
  }
  return Finish(sequence);
}

Expression* Parser::ParseAssignment(bool no_in) {
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return nullptr;
  }
  if (has_error()) {
    return nullptr;
  }
  if (CheckName("yield") &&
      (function_state_.is_generator || function_state_.strict)) {
    if (function_state_.is_generator) {
      return ParseYield(no_in);
    }
    // `yield` in strict non-generator code is a reserved word.
    FailUnexpected();
    return nullptr;
  }
  int32_t start = start_pos();
  // Single-identifier arrow function: x => ...
  if (Check(Tok::kName) && IsIdentifierToken() && !CheckName("async")) {
    Token next = PeekNext();
    if (next.type == Tok::kArrow && !next.newline_before) {
      Identifier* param = ParseBindingIdentifier();
      if (param == nullptr) {
        return nullptr;
      }
      Advance();  // =>
      return ParseArrowFromParams({param}, start, false);
    }
  }
  if (CheckName("async")) {
    // async x => ... (async function/arrow with parens are handled by the
    // primary expression parser), or `async => ...` with async as the
    // parameter name.
    Token next = PeekNext();
    if (next.type == Tok::kArrow && !next.newline_before) {
      Identifier* param = ParseBindingIdentifier();
      if (param == nullptr) {
        return nullptr;
      }
      Advance();  // =>
      return ParseArrowFromParams({param}, start, false);
    }
    if (next.type == Tok::kName && !next.newline_before) {
      Token next2 = PeekNext2();
      if (next2.type == Tok::kArrow && !next2.newline_before) {
        Advance();  // async
        function_stack_.push_back(function_state_);
        function_state_.is_async = true;
        Identifier* param = ParseBindingIdentifier();
        function_state_ = function_stack_.back();
        function_stack_.pop_back();
        if (param == nullptr) {
          return nullptr;
        }
        Advance();  // =>
        return ParseArrowFromParams({param}, start, true);
      }
    }
  }

  Expression* left = ParseConditional(no_in);
  if (left == nullptr) {
    return nullptr;
  }
  AssignOp op;
  if (!AssignOpOfToken(peek_type(), &op)) {
    return left;
  }
  if (IsBareArrow(left)) {
    FailUnexpected();
    return nullptr;
  }
  if (op == AssignOp::kAssign &&
      (left->Is<ObjectExpression>() || left->Is<ArrayExpression>())) {
    left = ToPattern(left, false);
    if (left == nullptr) {
      return nullptr;
    }
  } else if (!IsValidSimpleAssignmentTarget(left)) {
    Fail("Invalid left-hand side in assignment", left->start);
    return nullptr;
  }
  Advance();  // operator
  AssignmentExpression* assignment = New<AssignmentExpression>(left->start);
  assignment->op = op;
  assignment->left = left;
  assignment->right = ParseAssignment(no_in);
  if (assignment->right == nullptr) {
    return nullptr;
  }
  return Finish(assignment);
}

Expression* Parser::ParseYield(bool no_in) {
  YieldExpression* yield = New<YieldExpression>(start_pos());
  Advance();  // yield
  if (cur().newline_before) {
    return Finish(yield);
  }
  if (Eat(Tok::kMul)) {
    yield->delegate = true;
    yield->argument = ParseAssignment(no_in);
    if (yield->argument == nullptr) {
      return nullptr;
    }
    return Finish(yield);
  }
  if (TokenStartsExpression()) {
    yield->argument = ParseAssignment(no_in);
    if (yield->argument == nullptr) {
      return nullptr;
    }
  }
  return Finish(yield);
}

Expression* Parser::ParseConditional(bool no_in) {
  int32_t start = start_pos();
  Expression* test = ParseBinary(0, no_in);
  if (test == nullptr) {
    return nullptr;
  }
  if (!Check(Tok::kQuestion)) {
    return test;
  }
  if (IsBareArrow(test)) {
    return test;
  }
  Advance();
  ConditionalExpression* conditional = New<ConditionalExpression>(start);
  conditional->test = test;
  conditional->consequent = ParseAssignment(false);
  if (conditional->consequent == nullptr) {
    return nullptr;
  }
  if (!Expect(Tok::kColon)) {
    return nullptr;
  }
  conditional->alternate = ParseAssignment(no_in);
  if (conditional->alternate == nullptr) {
    return nullptr;
  }
  return Finish(conditional);
}

Expression* Parser::ParseBinary(int min_precedence, bool no_in) {
  // The right operand of a right-associative operator (`**`) recurses into
  // ParseBinary, so this frame counts toward the nesting depth.
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return nullptr;
  }
  int32_t start = start_pos();
  Expression* left;
  if (Check(Tok::kPrivateName)) {
    // `#x in obj`
    Token next = PeekNext();
    if (next.type != Tok::kIn || no_in) {
      FailUnexpected();
      return nullptr;
    }
    PrivateIdentifier* id = New<PrivateIdentifier>(start);
    id->name = Intern(cur().value);
    Advance();
    left = Finish(id);
  } else {
    left = ParseUnary();
    if (left == nullptr) {
      return nullptr;
    }
    if (IsBareArrow(left)) {
      return left;
    }
  }
  int chain_depth = 0;
  while (true) {
    BinaryOp bin_op = BinaryOp::kAdd;
    LogicalOp log_op = LogicalOp::kAnd;
    bool is_logical = false;
    int precedence = BinaryPrecedenceOfToken(peek_type(), no_in, &bin_op,
                                             &log_op, &is_logical);
    if (precedence == 0 || precedence <= min_precedence) {
      break;
    }
    if (depth_ + ++chain_depth > kMaxDepth) {
      Fail("Maximum nesting depth exceeded", start_pos());
      return nullptr;
    }
    Advance();
    // `**` is right-associative.
    int right_min =
        bin_op == BinaryOp::kExp && !is_logical ? precedence - 1 : precedence;
    Expression* right = ParseBinary(right_min, no_in);
    if (right == nullptr) {
      return nullptr;
    }
    if (is_logical) {
      LogicalExpression* logical = New<LogicalExpression>(start);
      logical->op = log_op;
      logical->left = left;
      logical->right = right;
      left = Finish(logical);
    } else {
      BinaryExpression* binary = New<BinaryExpression>(start);
      binary->op = bin_op;
      binary->left = left;
      binary->right = right;
      left = Finish(binary);
    }
  }
  return left;
}

Expression* Parser::ParseUnary() {
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return nullptr;
  }
  int32_t start = start_pos();
  UnaryOp op;
  bool is_unary = true;
  switch (peek_type()) {
    case Tok::kSub:
      op = UnaryOp::kMinus;
      break;
    case Tok::kAdd:
      op = UnaryOp::kPlus;
      break;
    case Tok::kNot:
      op = UnaryOp::kNot;
      break;
    case Tok::kBitNot:
      op = UnaryOp::kBitNot;
      break;
    case Tok::kTypeof:
      op = UnaryOp::kTypeof;
      break;
    case Tok::kVoid:
      op = UnaryOp::kVoid;
      break;
    case Tok::kDelete:
      op = UnaryOp::kDelete;
      break;
    default:
      is_unary = false;
      op = UnaryOp::kMinus;
      break;
  }
  if (is_unary) {
    Advance();
    UnaryExpression* unary = New<UnaryExpression>(start);
    unary->op = op;
    unary->argument = ParseUnary();
    if (unary->argument == nullptr) {
      return nullptr;
    }
    return Finish(unary);
  }
  if (Check(Tok::kInc) || Check(Tok::kDec)) {
    UpdateExpression* update = New<UpdateExpression>(start);
    update->op = Check(Tok::kInc) ? UpdateOp::kIncrement : UpdateOp::kDecrement;
    update->prefix = true;
    Advance();
    update->argument = ParseUnary();
    if (update->argument == nullptr) {
      return nullptr;
    }
    if (!IsValidSimpleAssignmentTarget(update->argument)) {
      Fail("Invalid left-hand side expression in prefix operation",
           update->argument->start);
      return nullptr;
    }
    return Finish(update);
  }
  if (CheckName("await") && function_state_.is_async) {
    AwaitExpression* await_expression = New<AwaitExpression>(start);
    Advance();
    await_expression->argument = ParseUnary();
    if (await_expression->argument == nullptr) {
      return nullptr;
    }
    return Finish(await_expression);
  }
  return ParsePostfix();
}

Expression* Parser::ParsePostfix() {
  int32_t start = start_pos();
  Expression* expression = ParseLeftHandSide();
  if (expression == nullptr) {
    return nullptr;
  }
  if ((Check(Tok::kInc) || Check(Tok::kDec)) && !cur().newline_before) {
    if (!IsValidSimpleAssignmentTarget(expression)) {
      Fail("Invalid left-hand side expression in postfix operation",
           expression->start);
      return nullptr;
    }
    UpdateExpression* update = New<UpdateExpression>(start);
    update->op = Check(Tok::kInc) ? UpdateOp::kIncrement : UpdateOp::kDecrement;
    update->prefix = false;
    update->argument = expression;
    Advance();
    return Finish(update);
  }
  return expression;
}

Expression* Parser::ParseLeftHandSide() {
  int32_t start = start_pos();
  Expression* base = ParsePrimary();
  if (base == nullptr) {
    return nullptr;
  }
  if (IsBareArrow(base)) {
    return base;
  }
  return ParseSubscripts(base, start, false);
}

Expression* Parser::ParseSubscripts(Expression* base, int32_t start,
                                    bool no_calls) {
  bool maybe_async_arrow = false;
  if (auto* id = base->As<Identifier>()) {
    maybe_async_arrow =
        id->name == "async" && !cur().newline_before && base->end == prev_end();
  }
  bool optional_chained = false;
  int chain_depth = 0;
  while (true) {
    if (has_error()) {
      return nullptr;
    }
    if (Check(Tok::kPeriod)) {
      Advance();
      MemberExpression* member = New<MemberExpression>(start);
      member->object = base;
      if (Check(Tok::kPrivateName)) {
        PrivateIdentifier* id = New<PrivateIdentifier>(start_pos());
        id->name = Intern(cur().value);
        Advance();
        member->property = Finish(id);
      } else {
        member->property = ParseIdentifierName();
        if (member->property == nullptr) {
          return nullptr;
        }
      }
      base = Finish(member);
    } else if (Check(Tok::kQuestionPeriod)) {
      Advance();
      optional_chained = true;
      if (Check(Tok::kLeftParen)) {
        if (no_calls) {
          FailUnexpected();
          return nullptr;
        }
        CallExpression* call = New<CallExpression>(start);
        call->callee = base;
        call->optional = true;
        if (!ParseArguments(&call->arguments)) {
          return nullptr;
        }
        base = Finish(call);
      } else if (Check(Tok::kLeftBracket)) {
        Advance();
        MemberExpression* member = New<MemberExpression>(start);
        member->object = base;
        member->computed = true;
        member->optional = true;
        member->property = ParseExpression();
        if (member->property == nullptr) {
          return nullptr;
        }
        if (!Expect(Tok::kRightBracket)) {
          return nullptr;
        }
        base = Finish(member);
      } else if (Check(Tok::kPrivateName)) {
        MemberExpression* member = New<MemberExpression>(start);
        member->object = base;
        member->optional = true;
        PrivateIdentifier* id = New<PrivateIdentifier>(start_pos());
        id->name = Intern(cur().value);
        Advance();
        member->property = Finish(id);
        base = Finish(member);
      } else {
        MemberExpression* member = New<MemberExpression>(start);
        member->object = base;
        member->optional = true;
        member->property = ParseIdentifierName();
        if (member->property == nullptr) {
          return nullptr;
        }
        base = Finish(member);
      }
    } else if (Check(Tok::kLeftBracket)) {
      Advance();
      MemberExpression* member = New<MemberExpression>(start);
      member->object = base;
      member->computed = true;
      member->property = ParseExpression();
      if (member->property == nullptr) {
        return nullptr;
      }
      if (!Expect(Tok::kRightBracket)) {
        return nullptr;
      }
      base = Finish(member);
    } else if (!no_calls && Check(Tok::kLeftParen)) {
      std::vector<Expression*> arguments;
      if (!ParseArguments(&arguments)) {
        return nullptr;
      }
      if (maybe_async_arrow && Check(Tok::kArrow) && !cur().newline_before) {
        Advance();  // =>
        return ParseArrowFromParams(std::move(arguments), start, true);
      }
      CallExpression* call = New<CallExpression>(start);
      call->callee = base;
      call->arguments = std::move(arguments);
      base = Finish(call);
    } else if (Check(Tok::kTemplateHead) ||
               Check(Tok::kNoSubstitutionTemplate)) {
      if (optional_chained) {
        Fail("Invalid tagged template on optional chain", start_pos());
        return nullptr;
      }
      TaggedTemplateExpression* tagged = New<TaggedTemplateExpression>(start);
      tagged->tag = base;
      Expression* quasi = ParseTemplateLiteral(true);
      if (quasi == nullptr) {
        return nullptr;
      }
      tagged->quasi = quasi->Cast<TemplateLiteral>();
      base = Finish(tagged);
    } else {
      break;
    }
    if (depth_ + ++chain_depth > kMaxDepth) {
      Fail("Maximum nesting depth exceeded", start_pos());
      return nullptr;
    }
    maybe_async_arrow = false;
  }
  if (optional_chained) {
    ChainExpression* chain = New<ChainExpression>(start);
    chain->expression = base;
    return Finish(chain);
  }
  return base;
}

bool Parser::ParseArguments(std::vector<Expression*>* arguments) {
  if (!Expect(Tok::kLeftParen)) {
    return false;
  }
  while (!Check(Tok::kRightParen)) {
    if (has_error()) {
      return false;
    }
    Expression* argument = ParseSpreadOrAssignment();
    if (argument == nullptr) {
      return false;
    }
    arguments->push_back(argument);
    if (!Check(Tok::kRightParen) && !Expect(Tok::kComma)) {
      return false;
    }
  }
  return Expect(Tok::kRightParen);
}

Expression* Parser::ParseSpreadOrAssignment() {
  if (Check(Tok::kEllipsis)) {
    SpreadElement* spread = New<SpreadElement>(start_pos());
    Advance();
    spread->argument = ParseAssignment();
    if (spread->argument == nullptr) {
      return nullptr;
    }
    return Finish(spread);
  }
  return ParseAssignment();
}

Expression* Parser::ParseNewExpression() {
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return nullptr;
  }
  int32_t start = start_pos();
  Advance();  // new
  if (Eat(Tok::kPeriod)) {
    if (!CheckName("target")) {
      FailUnexpected();
      return nullptr;
    }
    Advance();
    MetaProperty* meta = New<MetaProperty>(start);
    meta->meta_kind = MetaPropertyKind::kNewTarget;
    return Finish(meta);
  }
  NewExpression* new_expression = New<NewExpression>(start);
  int32_t callee_start = start_pos();
  Expression* callee;
  if (Check(Tok::kNew)) {
    callee = ParseNewExpression();
  } else if (Check(Tok::kImport)) {
    Fail("Cannot use new with import", start_pos());
    return nullptr;
  } else {
    callee = ParsePrimary();
  }
  if (callee == nullptr) {
    return nullptr;
  }
  callee = ParseSubscripts(callee, callee_start, true);
  if (callee == nullptr) {
    return nullptr;
  }
  if (callee->Is<ChainExpression>()) {
    Fail("Invalid optional chain from new expression", callee->start);
    return nullptr;
  }
  new_expression->callee = callee;
  if (Check(Tok::kLeftParen) && !ParseArguments(&new_expression->arguments)) {
    return nullptr;
  }
  return Finish(new_expression);
}

Expression* Parser::ParseSuper() {
  Super* super = New<Super>(start_pos());
  Advance();
  if (!Check(Tok::kLeftParen) && !Check(Tok::kPeriod) &&
      !Check(Tok::kLeftBracket)) {
    Fail("'super' keyword unexpected here", super->start);
    return nullptr;
  }
  return Finish(super);
}

Expression* Parser::ParseImportExpressionOrMeta() {
  int32_t start = start_pos();
  Advance();  // import
  bool is_defer = false;
  if (Eat(Tok::kPeriod)) {
    if (CheckName("defer")) {
      Advance();
      is_defer = true;
    } else {
      if (!ExpectName("meta")) {
        return nullptr;
      }
      MetaProperty* meta = New<MetaProperty>(start);
      meta->meta_kind = MetaPropertyKind::kImportMeta;
      return Finish(meta);
    }
  }
  ImportExpression* import = New<ImportExpression>(start);
  import->is_defer = is_defer;
  if (!Expect(Tok::kLeftParen)) {
    return nullptr;
  }
  import->source = ParseAssignment();
  if (import->source == nullptr) {
    return nullptr;
  }
  if (Eat(Tok::kComma) && !Check(Tok::kRightParen)) {
    import->options = ParseAssignment();
    if (import->options == nullptr) {
      return nullptr;
    }
    Eat(Tok::kComma);
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  return Finish(import);
}

Expression* Parser::ParseNativeCall() {
  NativeCallExpression* call = New<NativeCallExpression>(start_pos());
  Advance();  // %
  if (!Check(Tok::kName) && !IsKeyword(peek_type())) {
    FailUnexpected();
    return nullptr;
  }
  call->name = Intern(cur().value);
  Advance();
  if (!ParseArguments(&call->arguments)) {
    return nullptr;
  }
  return Finish(call);
}

Expression* Parser::ParsePrimary() {
  int32_t start = start_pos();
  switch (peek_type()) {
    case Tok::kThis:
      Advance();
      return Finish(New<ThisExpression>(start));
    case Tok::kNull:
      Advance();
      return Finish(New<NullLiteral>(start));
    case Tok::kTrue:
    case Tok::kFalse: {
      BooleanLiteral* literal = New<BooleanLiteral>(start);
      literal->value = Check(Tok::kTrue);
      Advance();
      return Finish(literal);
    }
    case Tok::kNumber: {
      NumericLiteral* literal = New<NumericLiteral>(start);
      literal->value = cur().number_value;
      literal->raw = Slice(cur().start, cur().end);
      Advance();
      return Finish(literal);
    }
    case Tok::kBigInt: {
      BigIntLiteral* literal = New<BigIntLiteral>(start);
      literal->raw = Slice(cur().start, cur().end);
      Advance();
      return Finish(literal);
    }
    case Tok::kString: {
      StringLiteral* literal = New<StringLiteral>(start);
      literal->value = cur().value;
      literal->raw = Slice(cur().start, cur().end);
      Advance();
      return Finish(literal);
    }
    case Tok::kDiv:
    case Tok::kAssignDiv: {
      if (!lexer_.RescanAsRegExp()) {
        Fail(lexer_.has_error() ? lexer_.error() : "Invalid regular expression",
             start);
        return nullptr;
      }
      RegExpLiteral* literal = New<RegExpLiteral>(start);
      literal->pattern = Intern(cur().value);
      literal->regexp_flags = Intern(cur().flags);
      Advance();
      return Finish(literal);
    }
    case Tok::kTemplateHead:
    case Tok::kNoSubstitutionTemplate:
      return ParseTemplateLiteral(false);
    case Tok::kLeftParen:
      return ParseParenthesized();
    case Tok::kLeftBracket:
      return ParseArrayLiteral();
    case Tok::kLeftBrace:
      return ParseObjectLiteral();
    case Tok::kFunction:
      return ParseFunctionExpression(start, false);
    case Tok::kClass: {
      ClassExpression* cls = New<ClassExpression>(start);
      if (!ParseClass(cls, start, true)) {
        return nullptr;
      }
      return Finish(cls);
    }
    case Tok::kNew:
      return ParseNewExpression();
    case Tok::kSuper:
      return ParseSuper();
    case Tok::kImport:
      return ParseImportExpressionOrMeta();
    case Tok::kMod:
      if (options_.allow_natives_syntax) {
        return ParseNativeCall();
      }
      FailUnexpected();
      return nullptr;
    case Tok::kName: {
      if (CheckName("async")) {
        Token next = PeekNext();
        if (next.type == Tok::kFunction && !next.newline_before) {
          Advance();  // async
          return ParseFunctionExpression(start, true);
        }
      }
      return ParseIdentifierReference();
    }
    default:
      FailUnexpected();
      return nullptr;
  }
}

Expression* Parser::ParseIdentifierReference() {
  if (!IsIdentifierToken()) {
    FailUnexpected();
    return nullptr;
  }
  Identifier* id = New<Identifier>(start_pos());
  id->name = Intern(cur().value);
  Advance();
  return Finish(id);
}

Identifier* Parser::ParseIdentifierName() {
  if (!Check(Tok::kName) && !IsKeyword(peek_type())) {
    FailUnexpected();
    return nullptr;
  }
  Identifier* id = New<Identifier>(start_pos());
  id->name = Intern(cur().value);
  Advance();
  return Finish(id);
}

Identifier* Parser::ParseBindingIdentifier() {
  if (!IsIdentifierToken()) {
    FailUnexpected();
    return nullptr;
  }
  Identifier* id = New<Identifier>(start_pos());
  id->name = Intern(cur().value);
  Advance();
  return Finish(id);
}

Expression* Parser::ParseBindingPattern() {
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", start_pos());
    return nullptr;
  }
  int32_t start = start_pos();
  if (Check(Tok::kLeftBracket)) {
    ArrayPattern* pattern = New<ArrayPattern>(start);
    Advance();
    while (!Check(Tok::kRightBracket)) {
      if (has_error()) {
        return nullptr;
      }
      if (Check(Tok::kComma)) {
        Advance();
        pattern->elements.push_back(nullptr);
        continue;
      }
      if (Check(Tok::kEllipsis)) {
        RestElement* rest = New<RestElement>(start_pos());
        Advance();
        rest->argument = ParseBindingPattern();
        if (rest->argument == nullptr) {
          return nullptr;
        }
        pattern->elements.push_back(Finish(rest));
        if (!Check(Tok::kRightBracket)) {
          FailUnexpected();
          return nullptr;
        }
        break;
      }
      Expression* element = ParseBindingElement();
      if (element == nullptr) {
        return nullptr;
      }
      pattern->elements.push_back(element);
      if (!Check(Tok::kRightBracket) && !Expect(Tok::kComma)) {
        return nullptr;
      }
    }
    if (!Expect(Tok::kRightBracket)) {
      return nullptr;
    }
    return Finish(pattern);
  }
  if (Check(Tok::kLeftBrace)) {
    ObjectPattern* pattern = New<ObjectPattern>(start);
    Advance();
    while (!Check(Tok::kRightBrace)) {
      if (has_error()) {
        return nullptr;
      }
      if (Check(Tok::kEllipsis)) {
        RestElement* rest = New<RestElement>(start_pos());
        Advance();
        rest->argument = ParseBindingIdentifier();
        if (rest->argument == nullptr) {
          return nullptr;
        }
        pattern->properties.push_back(Finish(rest));
        if (!Check(Tok::kRightBrace)) {
          FailUnexpected();
          return nullptr;
        }
        break;
      }
      Property* property = New<Property>(start_pos());
      bool computed = false;
      bool key_is_plain_name = Check(Tok::kName);
      property->key = ParsePropertyKey(&computed, false);
      if (property->key == nullptr) {
        return nullptr;
      }
      property->computed = computed;
      if (Eat(Tok::kColon)) {
        property->value = ParseBindingElement();
        if (property->value == nullptr) {
          return nullptr;
        }
      } else {
        if (!key_is_plain_name || computed) {
          FailUnexpected();
          return nullptr;
        }
        auto* key = property->key->Cast<Identifier>();
        if (IsReservedWordInContext(key->name)) {
          Fail("Unexpected reserved word", key->start);
          return nullptr;
        }
        property->shorthand = true;
        Identifier* value = New<Identifier>(key->start);
        value->name = key->name;
        value->end = key->end;
        if (Check(Tok::kAssign)) {
          Advance();
          AssignmentPattern* with_default = New<AssignmentPattern>(key->start);
          with_default->left = value;
          with_default->right = ParseAssignment();
          if (with_default->right == nullptr) {
            return nullptr;
          }
          property->value = Finish(with_default);
        } else {
          property->value = value;
        }
      }
      pattern->properties.push_back(Finish(property));
      if (!Check(Tok::kRightBrace) && !Expect(Tok::kComma)) {
        return nullptr;
      }
    }
    if (!Expect(Tok::kRightBrace)) {
      return nullptr;
    }
    return Finish(pattern);
  }
  return ParseBindingIdentifier();
}

Expression* Parser::ParseBindingElement() {
  int32_t start = start_pos();
  Expression* target = ParseBindingPattern();
  if (target == nullptr) {
    return nullptr;
  }
  if (!Check(Tok::kAssign)) {
    return target;
  }
  Advance();
  AssignmentPattern* pattern = New<AssignmentPattern>(start);
  pattern->left = target;
  pattern->right = ParseAssignment();
  if (pattern->right == nullptr) {
    return nullptr;
  }
  return Finish(pattern);
}

Expression* Parser::ParseParenthesized() {
  int32_t start = start_pos();
  Advance();  // (
  std::vector<Expression*> items;
  bool has_rest = false;
  bool trailing_comma = false;
  int32_t trailing_comma_pos = -1;
  while (!Check(Tok::kRightParen)) {
    if (has_error()) {
      return nullptr;
    }
    if (Check(Tok::kEllipsis)) {
      SpreadElement* spread = New<SpreadElement>(start_pos());
      Advance();
      spread->argument = ParseBindingPattern();
      if (spread->argument == nullptr) {
        return nullptr;
      }
      items.push_back(Finish(spread));
      has_rest = true;
      if (!Check(Tok::kRightParen)) {
        FailUnexpected();
        return nullptr;
      }
      break;
    }
    Expression* item = ParseAssignment();
    if (item == nullptr) {
      return nullptr;
    }
    items.push_back(item);
    if (Check(Tok::kRightParen)) {
      break;
    }
    trailing_comma_pos = start_pos();
    if (!Expect(Tok::kComma)) {
      return nullptr;
    }
    trailing_comma = Check(Tok::kRightParen);
  }
  if (!Expect(Tok::kRightParen)) {
    return nullptr;
  }
  if (Check(Tok::kArrow) && !cur().newline_before) {
    Advance();  // =>
    return ParseArrowFromParams(std::move(items), start, false);
  }
  if (items.empty() || has_rest || trailing_comma) {
    Fail("Unexpected token",
         has_rest || items.empty() ? prev_end() : trailing_comma_pos);
    return nullptr;
  }
  if (items.size() == 1) {
    return items[0];
  }
  SequenceExpression* sequence = New<SequenceExpression>(items[0]->start);
  sequence->expressions = std::move(items);
  sequence->end = sequence->expressions.back()->end;
  return sequence;
}

Expression* Parser::ParseArrayLiteral() {
  ArrayExpression* array = New<ArrayExpression>(start_pos());
  Advance();  // [
  while (!Check(Tok::kRightBracket)) {
    if (has_error()) {
      return nullptr;
    }
    if (Check(Tok::kComma)) {
      Advance();
      array->elements.push_back(nullptr);
      continue;
    }
    Expression* element = ParseSpreadOrAssignment();
    if (element == nullptr) {
      return nullptr;
    }
    array->elements.push_back(element);
    if (!Check(Tok::kRightBracket) && !Expect(Tok::kComma)) {
      return nullptr;
    }
  }
  Advance();  // ]
  return Finish(array);
}

Expression* Parser::ParseObjectLiteral() {
  ObjectExpression* object = New<ObjectExpression>(start_pos());
  Advance();  // {
  while (!Check(Tok::kRightBrace)) {
    if (has_error()) {
      return nullptr;
    }
    Node* property = ParseObjectProperty();
    if (property == nullptr) {
      return nullptr;
    }
    object->properties.push_back(property);
    if (!Check(Tok::kRightBrace) && !Expect(Tok::kComma)) {
      return nullptr;
    }
  }
  Advance();  // }
  return Finish(object);
}

Node* Parser::ParseObjectProperty() {
  int32_t start = start_pos();
  if (Check(Tok::kEllipsis)) {
    SpreadElement* spread = New<SpreadElement>(start);
    Advance();
    spread->argument = ParseAssignment();
    if (spread->argument == nullptr) {
      return nullptr;
    }
    return Finish(spread);
  }
  Property* property = New<Property>(start);
  bool is_async = false;
  bool is_generator = false;
  if (CheckName("get") || CheckName("set") || CheckName("async")) {
    Token next = PeekNext();
    bool is_modifier =
        !(next.type == Tok::kComma || next.type == Tok::kColon ||
          next.type == Tok::kLeftParen || next.type == Tok::kRightBrace ||
          next.type == Tok::kAssign ||
          (cur().value == "async" && next.newline_before));
    if (is_modifier) {
      if (cur().value == "get") {
        property->property_kind = PropertyKind::kGet;
      } else if (cur().value == "set") {
        property->property_kind = PropertyKind::kSet;
      } else {
        is_async = true;
      }
      Advance();
    }
  }
  if (Check(Tok::kMul)) {
    is_generator = true;
    Advance();
  }
  bool computed = false;
  bool key_is_plain_name = Check(Tok::kName);
  int32_t key_start = start_pos();
  property->key = ParsePropertyKey(&computed, false);
  if (property->key == nullptr) {
    return nullptr;
  }
  property->computed = computed;

  bool is_accessor = property->property_kind != PropertyKind::kInit;
  if (Check(Tok::kLeftParen) || is_accessor || is_async || is_generator) {
    FunctionExpression* fn = New<FunctionExpression>(key_start);
    fn->is_async = is_async;
    fn->is_generator = is_generator;
    if (!ParseFunctionRest(
            fn, key_start, true, property->property_kind == PropertyKind::kGet,
            property->property_kind == PropertyKind::kSet, false)) {
      return nullptr;
    }
    property->value = Finish(fn);
    property->method = !is_accessor;
    return Finish(property);
  }
  if (Eat(Tok::kColon)) {
    property->value = ParseAssignment();
    if (property->value == nullptr) {
      return nullptr;
    }
    return Finish(property);
  }
  // Shorthand `{a}` or cover-initialized `{a = 1}` (only valid as a pattern).
  if (!key_is_plain_name || computed) {
    FailUnexpected();
    return nullptr;
  }
  auto* key = property->key->Cast<Identifier>();
  if (IsReservedWordInContext(key->name)) {
    Fail("Unexpected reserved word", key->start);
    return nullptr;
  }
  property->shorthand = true;
  Identifier* value = New<Identifier>(key->start);
  value->name = key->name;
  value->end = key->end;
  if (Check(Tok::kAssign)) {
    Advance();
    AssignmentPattern* with_default = New<AssignmentPattern>(key->start);
    with_default->left = value;
    with_default->right = ParseAssignment();
    if (with_default->right == nullptr) {
      return nullptr;
    }
    property->value = Finish(with_default);
  } else {
    property->value = value;
  }
  return Finish(property);
}

Expression* Parser::ParseTemplateLiteral(bool tagged) {
  TemplateLiteral* literal = New<TemplateLiteral>(start_pos());
  auto add_quasi = [&](const Token& token) -> bool {
    TemplateElement* element = New<TemplateElement>(token.start);
    bool is_tail = token.type == Tok::kNoSubstitutionTemplate ||
                   token.type == Tok::kTemplateTail;
    // Strip the delimiters: a leading '`' or '}' and a trailing '`' or '${'.
    int32_t raw_start = token.start + 1;
    int32_t raw_end = token.end - (is_tail ? 1 : 2);
    element->raw = Slice(raw_start, raw_end);
    element->cooked = token.value;
    element->cooked_valid = token.cooked_valid;
    element->tail = is_tail;
    if (!tagged && !token.cooked_valid) {
      Fail("Invalid escape sequence in template", token.start);
      return false;
    }
    element->end = token.end;
    literal->quasis.push_back(element);
    return true;
  };
  Token token = cur();
  if (!add_quasi(token)) {
    return nullptr;
  }
  if (token.type == Tok::kNoSubstitutionTemplate) {
    Advance();
    return Finish(literal);
  }
  Advance();  // past the head
  while (true) {
    if (has_error()) {
      return nullptr;
    }
    Expression* expression = ParseExpression();
    if (expression == nullptr) {
      return nullptr;
    }
    literal->expressions.push_back(expression);
    if (!Check(Tok::kRightBrace)) {
      FailUnexpected();
      return nullptr;
    }
    if (!lexer_.RescanAsTemplateContinuation()) {
      Fail(lexer_.has_error() ? lexer_.error() : "Unterminated template",
           start_pos());
      return nullptr;
    }
    token = cur();
    if (!add_quasi(token)) {
      return nullptr;
    }
    Advance();
    if (token.type == Tok::kTemplateTail) {
      break;
    }
  }
  return Finish(literal);
}

Expression* Parser::ParseFunctionExpression(int32_t start, bool is_async) {
  FunctionExpression* fn = New<FunctionExpression>(start);
  fn->is_async = is_async;
  Advance();  // function
  fn->is_generator = Eat(Tok::kMul);
  if (!Check(Tok::kLeftParen)) {
    // The name of a function expression is bound inside the function, so
    // yield/await keyword-ness follows the function's own kind.
    function_stack_.push_back(function_state_);
    function_state_.is_async = is_async;
    function_state_.is_generator = fn->is_generator;
    function_state_.in_static_block = false;
    fn->name = ParseBindingIdentifier();
    function_state_ = function_stack_.back();
    function_stack_.pop_back();
    if (fn->name == nullptr) {
      return nullptr;
    }
  }
  if (!ParseFunctionRest(fn, start, false, false, false, false)) {
    return nullptr;
  }
  return Finish(fn);
}

// ---------------------------------------------------------------------------
// Cover grammar

bool Parser::IsValidSimpleAssignmentTarget(Expression* expression) const {
  switch (expression->kind) {
    case NodeKind::kIdentifier:
    case NodeKind::kMemberExpression:
    // Web compat: V8 accepts `f() = 1`, `f() += 1`, `f()++` and
    // `for (f() in o)` in any mode and throws a ReferenceError at runtime.
    case NodeKind::kCallExpression:
      return true;
    default:
      return false;
  }
}

Expression* Parser::ToPattern(Expression* expression, bool binding) {
  DepthGuard guard(&depth_);
  if (depth_ > kMaxDepth) {
    Fail("Maximum nesting depth exceeded", expression->start);
    return nullptr;
  }
  switch (expression->kind) {
    case NodeKind::kIdentifier:
      return expression;
    case NodeKind::kMemberExpression:
      if (binding) {
        break;
      }
      return expression;
    case NodeKind::kObjectPattern:
    case NodeKind::kArrayPattern:
    case NodeKind::kAssignmentPattern:
    case NodeKind::kRestElement:
      return expression;
    case NodeKind::kObjectExpression: {
      auto* object = expression->Cast<ObjectExpression>();
      ObjectPattern* pattern = New<ObjectPattern>(object->start);
      pattern->end = object->end;
      for (size_t i = 0; i < object->properties.size(); i++) {
        Node* entry = object->properties[i];
        if (auto* spread = entry->As<SpreadElement>()) {
          if (i + 1 != object->properties.size()) {
            Fail("Rest element must be last element", spread->start);
            return nullptr;
          }
          RestElement* rest = New<RestElement>(spread->start);
          rest->end = spread->end;
          rest->argument = ToPattern(spread->argument, binding);
          if (rest->argument == nullptr) {
            return nullptr;
          }
          pattern->properties.push_back(rest);
          continue;
        }
        auto* property = entry->Cast<Property>();
        if (property->method ||
            property->property_kind != PropertyKind::kInit) {
          Fail("Invalid destructuring assignment target", property->start);
          return nullptr;
        }
        property->value = ToPattern(property->value, binding);
        if (property->value == nullptr) {
          return nullptr;
        }
        pattern->properties.push_back(property);
      }
      return pattern;
    }
    case NodeKind::kArrayExpression: {
      auto* array = expression->Cast<ArrayExpression>();
      ArrayPattern* pattern = New<ArrayPattern>(array->start);
      pattern->end = array->end;
      for (size_t i = 0; i < array->elements.size(); i++) {
        Expression* element = array->elements[i];
        if (element == nullptr) {
          pattern->elements.push_back(nullptr);
          continue;
        }
        if (auto* spread = element->As<SpreadElement>()) {
          if (i + 1 != array->elements.size()) {
            Fail("Rest element must be last element", spread->start);
            return nullptr;
          }
          RestElement* rest = New<RestElement>(spread->start);
          rest->end = spread->end;
          rest->argument = ToPattern(spread->argument, binding);
          if (rest->argument == nullptr) {
            return nullptr;
          }
          pattern->elements.push_back(rest);
          continue;
        }
        Expression* converted = ToPattern(element, binding);
        if (converted == nullptr) {
          return nullptr;
        }
        pattern->elements.push_back(converted);
      }
      return pattern;
    }
    case NodeKind::kAssignmentExpression: {
      auto* assignment = expression->Cast<AssignmentExpression>();
      if (assignment->op != AssignOp::kAssign) {
        break;
      }
      AssignmentPattern* pattern = New<AssignmentPattern>(assignment->start);
      pattern->end = assignment->end;
      pattern->left = ToPattern(assignment->left, binding);
      if (pattern->left == nullptr) {
        return nullptr;
      }
      pattern->right = assignment->right;
      return pattern;
    }
    default:
      break;
  }
  Fail(binding ? "Invalid destructuring assignment target"
               : "Invalid left-hand side in assignment",
       expression->start);
  return nullptr;
}

}  // namespace fast
}  // namespace redline
