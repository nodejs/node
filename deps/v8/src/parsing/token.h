// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_TOKEN_H_
#define V8_PARSING_TOKEN_H_

#include "src/base/logging.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// TOKEN_LIST takes a list of 3 macros M, all of which satisfy the
// same signature M(name, string, precedence), where name is the
// symbolic token name, string is the corresponding syntactic symbol
// (or nullptr, for literals), and precedence is the precedence (or 0).
// The parameters are invoked for token categories as follows:
//
//   T: Non-keyword tokens
//   K: Keyword tokens
//   C: Contextual keyword token
//
// Contextual keyword tokens are tokens that are scanned as Token::IDENTIFIER,
// but that in some contexts are treated as keywords. This mostly happens
// when ECMAScript introduces new keywords, but for backwards compatibility
// allows them to still be used as indentifiers in most contexts.

// IGNORE_TOKEN is a convenience macro that can be supplied as
// an argument (at any position) for a TOKEN_LIST call. It does
// nothing with tokens belonging to the respective category.

#define IGNORE_TOKEN(name, string, precedence)

/* Binary operators sorted by precedence */
#define BINARY_OP_TOKEN_LIST(T, E) \
  E(T, BIT_OR, "|", 6)             \
  E(T, BIT_XOR, "^", 7)            \
  E(T, BIT_AND, "&", 8)            \
  E(T, SHL, "<<", 11)              \
  E(T, SAR, ">>", 11)              \
  E(T, SHR, ">>>", 11)             \
  E(T, ADD, "+", 12)               \
  E(T, SUB, "-", 12)               \
  E(T, MUL, "*", 13)               \
  E(T, DIV, "/", 13)               \
  E(T, MOD, "%", 13)               \
  E(T, EXP, "**", 14)

#define EXPAND_BINOP_ASSIGN_TOKEN(T, name, string, precedence) \
  T(ASSIGN_##name, string "=", 2)

#define EXPAND_BINOP_TOKEN(T, name, string, precedence) \
  T(name, string, precedence)

#define TOKEN_LIST(T, K, C)                                        \
  /* End of source indicator. */                                   \
  T(EOS, "EOS", 0)                                                 \
                                                                   \
  /* Punctuators (ECMA-262, section 7.7, page 15). */              \
  T(LPAREN, "(", 0)                                                \
  T(RPAREN, ")", 0)                                                \
  T(LBRACK, "[", 0)                                                \
  T(RBRACK, "]", 0)                                                \
  T(LBRACE, "{", 0)                                                \
  T(RBRACE, "}", 0)                                                \
  T(COLON, ":", 0)                                                 \
  T(SEMICOLON, ";", 0)                                             \
  T(PERIOD, ".", 0)                                                \
  T(ELLIPSIS, "...", 0)                                            \
  T(CONDITIONAL, "?", 3)                                           \
  T(INC, "++", 0)                                                  \
  T(DEC, "--", 0)                                                  \
  T(ARROW, "=>", 0)                                                \
                                                                   \
  /* Assignment operators. */                                      \
  /* IsAssignmentOp() relies on this block of enum values being */ \
  /* contiguous and sorted in the same order! */                   \
  T(INIT, "=init", 2) /* AST-use only. */                          \
  T(ASSIGN, "=", 2)                                                \
  BINARY_OP_TOKEN_LIST(T, EXPAND_BINOP_ASSIGN_TOKEN)               \
                                                                   \
  /* Binary operators sorted by precedence. */                     \
  /* IsBinaryOp() relies on this block of enum values */           \
  /* being contiguous and sorted in the same order! */             \
  T(COMMA, ",", 1)                                                 \
  T(OR, "||", 4)                                                   \
  T(AND, "&&", 5)                                                  \
  BINARY_OP_TOKEN_LIST(T, EXPAND_BINOP_TOKEN)                      \
                                                                   \
  /* Compare operators sorted by precedence. */                    \
  /* IsCompareOp() relies on this block of enum values */          \
  /* being contiguous and sorted in the same order! */             \
  T(EQ, "==", 9)                                                   \
  T(EQ_STRICT, "===", 9)                                           \
  T(NE, "!=", 9)                                                   \
  T(NE_STRICT, "!==", 9)                                           \
  T(LT, "<", 10)                                                   \
  T(GT, ">", 10)                                                   \
  T(LTE, "<=", 10)                                                 \
  T(GTE, ">=", 10)                                                 \
  K(INSTANCEOF, "instanceof", 10)                                  \
  K(IN, "in", 10)                                                  \
                                                                   \
  /* Unary operators. */                                           \
  /* IsUnaryOp() relies on this block of enum values */            \
  /* being contiguous and sorted in the same order! */             \
  T(NOT, "!", 0)                                                   \
  T(BIT_NOT, "~", 0)                                               \
  K(DELETE, "delete", 0)                                           \
  K(TYPEOF, "typeof", 0)                                           \
  K(VOID, "void", 0)                                               \
                                                                   \
  /* Keywords (ECMA-262, section 7.5.2, page 13). */               \
  K(BREAK, "break", 0)                                             \
  K(CASE, "case", 0)                                               \
  K(CATCH, "catch", 0)                                             \
  K(CONTINUE, "continue", 0)                                       \
  K(DEBUGGER, "debugger", 0)                                       \
  K(DEFAULT, "default", 0)                                         \
  /* DELETE */                                                     \
  K(DO, "do", 0)                                                   \
  K(ELSE, "else", 0)                                               \
  K(FINALLY, "finally", 0)                                         \
  K(FOR, "for", 0)                                                 \
  K(FUNCTION, "function", 0)                                       \
  K(IF, "if", 0)                                                   \
  /* IN */                                                         \
  /* INSTANCEOF */                                                 \
  K(NEW, "new", 0)                                                 \
  K(RETURN, "return", 0)                                           \
  K(SWITCH, "switch", 0)                                           \
  K(THROW, "throw", 0)                                             \
  K(TRY, "try", 0)                                                 \
  /* TYPEOF */                                                     \
  K(VAR, "var", 0)                                                 \
  /* VOID */                                                       \
  K(WHILE, "while", 0)                                             \
  K(WITH, "with", 0)                                               \
  K(THIS, "this", 0)                                               \
                                                                   \
  /* Literals (ECMA-262, section 7.8, page 16). */                 \
  K(NULL_LITERAL, "null", 0)                                       \
  K(TRUE_LITERAL, "true", 0)                                       \
  K(FALSE_LITERAL, "false", 0)                                     \
  T(NUMBER, nullptr, 0)                                            \
  T(SMI, nullptr, 0)                                               \
  T(BIGINT, nullptr, 0)                                            \
  T(STRING, nullptr, 0)                                            \
                                                                   \
  /* BEGIN AnyIdentifier */                                        \
  /* Identifiers (not keywords or future reserved words). */       \
  T(IDENTIFIER, nullptr, 0)                                        \
  K(ASYNC, "async", 0)                                             \
  /* `await` is a reserved word in module code only */             \
  K(AWAIT, "await", 0)                                             \
  K(YIELD, "yield", 0)                                             \
  K(LET, "let", 0)                                                 \
  K(STATIC, "static", 0)                                           \
  /* Future reserved words (ECMA-262, section 7.6.1.2). */         \
  T(FUTURE_STRICT_RESERVED_WORD, nullptr, 0)                       \
  T(ESCAPED_STRICT_RESERVED_WORD, nullptr, 0)                      \
  K(ENUM, "enum", 0)                                               \
  /* END AnyIdentifier */                                          \
  K(CLASS, "class", 0)                                             \
  K(CONST, "const", 0)                                             \
  K(EXPORT, "export", 0)                                           \
  K(EXTENDS, "extends", 0)                                         \
  K(IMPORT, "import", 0)                                           \
  K(SUPER, "super", 0)                                             \
  T(PRIVATE_NAME, nullptr, 0)                                      \
                                                                   \
  /* Illegal token - not able to scan. */                          \
  T(ILLEGAL, "ILLEGAL", 0)                                         \
  T(ESCAPED_KEYWORD, nullptr, 0)                                   \
                                                                   \
  /* Scanner-internal use only. */                                 \
  T(WHITESPACE, nullptr, 0)                                        \
  T(UNINITIALIZED, nullptr, 0)                                     \
  T(REGEXP_LITERAL, nullptr, 0)                                    \
                                                                   \
  /* ES6 Template Literals */                                      \
  T(TEMPLATE_SPAN, nullptr, 0)                                     \
  T(TEMPLATE_TAIL, nullptr, 0)                                     \
                                                                   \
  /* Contextual keyword tokens */                                  \
  C(GET, "get", 0)                                                 \
  C(SET, "set", 0)                                                 \
  C(OF, "of", 0)                                                   \
  C(TARGET, "target", 0)                                           \
  C(META, "meta", 0)                                               \
  C(AS, "as", 0)                                                   \
  C(FROM, "from", 0)                                               \
  C(NAME, "name", 0)                                               \
  C(PROTO_UNDERSCORED, "__proto__", 0)                             \
  C(CONSTRUCTOR, "constructor", 0)                                 \
  C(PRIVATE_CONSTRUCTOR, "#constructor", 0)                        \
  C(PROTOTYPE, "prototype", 0)                                     \
  C(EVAL, "eval", 0)                                               \
  C(ARGUMENTS, "arguments", 0)                                     \
  C(UNDEFINED, "undefined", 0)                                     \
  C(ANONYMOUS, "anonymous", 0)

class Token {
 public:
  // All token values.
#define T(name, string, precedence) name,
  enum Value : uint8_t { TOKEN_LIST(T, T, T) NUM_TOKENS };
#undef T

  // Returns a string corresponding to the C++ token name
  // (e.g. "LT" for the token LT).
  static const char* Name(Value token) {
    DCHECK_GT(NUM_TOKENS, token);  // token is unsigned
    return name_[token];
  }

  static char TypeForTesting(Value token) { return token_type[token]; }

  // Predicates
  static bool IsKeyword(Value token) { return token_type[token] == 'K'; }
  static bool IsContextualKeyword(Value token) {
    return IsInRange(token, GET, ANONYMOUS);
  }

  static bool IsIdentifier(Value token, LanguageMode language_mode,
                           bool is_generator, bool disallow_await) {
    if (IsInRange(token, IDENTIFIER, ASYNC)) return true;
    if (IsInRange(token, LET, ESCAPED_STRICT_RESERVED_WORD)) {
      return is_sloppy(language_mode);
    }
    if (token == AWAIT) return !disallow_await;
    if (token == YIELD) return !is_generator && is_sloppy(language_mode);
    return false;
  }

  static bool IsAnyIdentifier(Value token) {
    return IsInRange(token, IDENTIFIER, ENUM);
  }

  static bool IsStrictReservedWord(Value token) {
    return IsInRange(token, LET, ESCAPED_STRICT_RESERVED_WORD);
  }

  static bool IsLiteral(Value token) {
    return IsInRange(token, NULL_LITERAL, STRING);
  }

  static bool IsAssignmentOp(Value token) {
    return IsInRange(token, INIT, ASSIGN_EXP);
  }
  static bool IsGetOrSet(Value op) { return IsInRange(op, GET, SET); }

  static bool IsBinaryOp(Value op) { return IsInRange(op, COMMA, EXP); }

  static bool IsCompareOp(Value op) { return IsInRange(op, EQ, IN); }

  static bool IsOrderedRelationalCompareOp(Value op) {
    return IsInRange(op, LT, GTE);
  }

  static bool IsEqualityOp(Value op) { return IsInRange(op, EQ, EQ_STRICT); }

  static Value BinaryOpForAssignment(Value op) {
    DCHECK(IsInRange(op, ASSIGN_BIT_OR, ASSIGN_EXP));
    Value result = static_cast<Value>(op - ASSIGN_BIT_OR + BIT_OR);
    DCHECK(IsBinaryOp(result));
    return result;
  }

  static bool IsBitOp(Value op) {
    return IsInRange(op, BIT_OR, SHR) || op == BIT_NOT;
  }

  static bool IsUnaryOp(Value op) {
    return IsInRange(op, NOT, VOID) || IsInRange(op, ADD, SUB);
  }

  static bool IsCountOp(Value op) { return IsInRange(op, INC, DEC); }

  static bool IsShiftOp(Value op) { return IsInRange(op, SHL, SHR); }

  static bool IsTrivialExpressionToken(Value op) {
    return IsInRange(op, THIS, IDENTIFIER);
  }

  // Returns a string corresponding to the JS token string
  // (.e., "<" for the token LT) or nullptr if the token doesn't
  // have a (unique) string (e.g. an IDENTIFIER).
  static const char* String(Value token) {
    DCHECK_GT(NUM_TOKENS, token);  // token is unsigned
    return string_[token];
  }

  static uint8_t StringLength(Value token) {
    DCHECK_GT(NUM_TOKENS, token);  // token is unsigned
    return string_length_[token];
  }

  // Returns the precedence > 0 for binary and compare
  // operators; returns 0 otherwise.
  static int Precedence(Value token) {
    DCHECK_GT(NUM_TOKENS, token);  // token is unsigned
    return precedence_[token];
  }

 private:
  static const char* const name_[NUM_TOKENS];
  static const char* const string_[NUM_TOKENS];
  static const uint8_t string_length_[NUM_TOKENS];
  static const int8_t precedence_[NUM_TOKENS];
  static const char token_type[NUM_TOKENS];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_TOKEN_H_
