// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_TOKEN_H_
#define V8_PARSING_TOKEN_H_

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/utils/utils.h"

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

// IGNORE_TOKEN is a convenience macro that can be supplied as
// an argument (at any position) for a TOKEN_LIST call. It does
// nothing with tokens belonging to the respective category.

#define IGNORE_TOKEN(name, string, precedence)

/* Binary operators */
/* ADD and SUB are at the end since they are UnaryOp */
#define BINARY_OP_TOKEN_LIST(T, E) \
  E(T, BIT_OR, "|", 6)             \
  E(T, BIT_XOR, "^", 7)            \
  E(T, BIT_AND, "&", 8)            \
  E(T, SHL, "<<", 11)              \
  E(T, SAR, ">>", 11)              \
  E(T, SHR, ">>>", 11)             \
  E(T, MUL, "*", 13)               \
  E(T, DIV, "/", 13)               \
  E(T, MOD, "%", 13)               \
  E(T, EXP, "**", 14)              \
  E(T, ADD, "+", 12)               \
  E(T, SUB, "-", 12)

#define EXPAND_BINOP_ASSIGN_TOKEN(T, name, string, precedence) \
  T(ASSIGN_##name, string "=", 2)

#define EXPAND_BINOP_TOKEN(T, name, string, precedence) \
  T(name, string, precedence)

#define TOKEN_LIST(T, K)                                           \
                                                                   \
  /* BEGIN PropertyOrCall */                                       \
  /* BEGIN Member */                                               \
  /* BEGIN Template */                                             \
  /* ES6 Template Literals */                                      \
  T(TEMPLATE_SPAN, nullptr, 0)                                     \
  T(TEMPLATE_TAIL, nullptr, 0)                                     \
  /* END Template */                                               \
                                                                   \
  /* Punctuators (ECMA-262, section 7.7, page 15). */              \
  /* BEGIN Property */                                             \
  T(PERIOD, ".", 0)                                                \
  T(LBRACK, "[", 0)                                                \
  /* END Property */                                               \
  /* END Member */                                                 \
  T(LPAREN, "(", 0)                                                \
  /* END PropertyOrCall */                                         \
  T(RPAREN, ")", 0)                                                \
  T(RBRACK, "]", 0)                                                \
  T(LBRACE, "{", 0)                                                \
  T(COLON, ":", 0)                                                 \
  T(ELLIPSIS, "...", 0)                                            \
  T(CONDITIONAL, "?", 3)                                           \
  /* BEGIN AutoSemicolon */                                        \
  T(SEMICOLON, ";", 0)                                             \
  T(RBRACE, "}", 0)                                                \
  /* End of source indicator. */                                   \
  T(EOS, "EOS", 0)                                                 \
  /* END AutoSemicolon */                                          \
                                                                   \
  /* BEGIN ArrowOrAssignmentOp */                                  \
  T(ARROW, "=>", 0)                                                \
  /* BEGIN AssignmentOp */                                         \
  /* IsAssignmentOp() relies on this block of enum values being */ \
  /* contiguous and sorted in the same order! */                   \
  T(INIT, "=init", 2) /* AST-use only. */                          \
  T(ASSIGN, "=", 2)                                                \
  BINARY_OP_TOKEN_LIST(T, EXPAND_BINOP_ASSIGN_TOKEN)               \
  /* END AssignmentOp */                                           \
  /* END ArrowOrAssignmentOp */                                    \
                                                                   \
  /* Binary operators sorted by precedence. */                     \
  /* IsBinaryOp() relies on this block of enum values */           \
  /* being contiguous and sorted in the same order! */             \
  T(COMMA, ",", 1)                                                 \
  T(OR, "||", 4)                                                   \
  T(AND, "&&", 5)                                                  \
                                                                   \
  /* Unary operators, starting at ADD in BINARY_OP_TOKEN_LIST  */  \
  /* IsUnaryOp() relies on this block of enum values */            \
  /* being contiguous and sorted in the same order! */             \
  BINARY_OP_TOKEN_LIST(T, EXPAND_BINOP_TOKEN)                      \
                                                                   \
  T(NOT, "!", 0)                                                   \
  T(BIT_NOT, "~", 0)                                               \
  K(DELETE, "delete", 0)                                           \
  K(TYPEOF, "typeof", 0)                                           \
  K(VOID, "void", 0)                                               \
                                                                   \
  /* BEGIN IsCountOp */                                            \
  T(INC, "++", 0)                                                  \
  T(DEC, "--", 0)                                                  \
  /* END IsCountOp */                                              \
  /* END IsUnaryOrCountOp */                                       \
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
  /* BEGIN Callable */                                             \
  K(SUPER, "super", 0)                                             \
  /* BEGIN AnyIdentifier */                                        \
  /* Identifiers (not keywords or future reserved words). */       \
  T(IDENTIFIER, nullptr, 0)                                        \
  K(GET, "get", 0)                                                 \
  K(SET, "set", 0)                                                 \
  K(ASYNC, "async", 0)                                             \
  /* `await` is a reserved word in module code only */             \
  K(AWAIT, "await", 0)                                             \
  K(YIELD, "yield", 0)                                             \
  K(LET, "let", 0)                                                 \
  K(STATIC, "static", 0)                                           \
  /* Future reserved words (ECMA-262, section 7.6.1.2). */         \
  T(FUTURE_STRICT_RESERVED_WORD, nullptr, 0)                       \
  T(ESCAPED_STRICT_RESERVED_WORD, nullptr, 0)                      \
  /* END AnyIdentifier */                                          \
  /* END Callable */                                               \
  K(ENUM, "enum", 0)                                               \
  K(CLASS, "class", 0)                                             \
  K(CONST, "const", 0)                                             \
  K(EXPORT, "export", 0)                                           \
  K(EXTENDS, "extends", 0)                                         \
  K(IMPORT, "import", 0)                                           \
  T(PRIVATE_NAME, nullptr, 0)                                      \
                                                                   \
  /* Illegal token - not able to scan. */                          \
  T(ILLEGAL, "ILLEGAL", 0)                                         \
  T(ESCAPED_KEYWORD, nullptr, 0)                                   \
                                                                   \
  /* Scanner-internal use only. */                                 \
  T(WHITESPACE, nullptr, 0)                                        \
  T(UNINITIALIZED, nullptr, 0)                                     \
  T(REGEXP_LITERAL, nullptr, 0)

class V8_EXPORT_PRIVATE Token {
 public:
  // All token values.
#define T(name, string, precedence) name,
  enum Value : uint8_t { TOKEN_LIST(T, T) NUM_TOKENS };
#undef T

  // Returns a string corresponding to the C++ token name
  // (e.g. "LT" for the token LT).
  static const char* Name(Value token) {
    DCHECK_GT(NUM_TOKENS, token);  // token is unsigned
    return name_[token];
  }

  class IsKeywordBits : public BitField8<bool, 0, 1> {};
  class IsPropertyNameBits : public BitField8<bool, IsKeywordBits::kNext, 1> {};

  // Predicates
  static bool IsKeyword(Value token) {
    return IsKeywordBits::decode(token_flags[token]);
  }

  static bool IsPropertyName(Value token) {
    return IsPropertyNameBits::decode(token_flags[token]);
  }

  V8_INLINE static bool IsValidIdentifier(Value token,
                                          LanguageMode language_mode,
                                          bool is_generator,
                                          bool disallow_await) {
    if (V8_LIKELY(IsInRange(token, IDENTIFIER, ASYNC))) return true;
    if (token == AWAIT) return !disallow_await;
    if (token == YIELD) return !is_generator && is_sloppy(language_mode);
    return IsStrictReservedWord(token) && is_sloppy(language_mode);
  }

  static bool IsCallable(Value token) {
    return IsInRange(token, SUPER, ESCAPED_STRICT_RESERVED_WORD);
  }

  static bool IsAutoSemicolon(Value token) {
    return IsInRange(token, SEMICOLON, EOS);
  }

  static bool IsAnyIdentifier(Value token) {
    return IsInRange(token, IDENTIFIER, ESCAPED_STRICT_RESERVED_WORD);
  }

  static bool IsStrictReservedWord(Value token) {
    return IsInRange(token, YIELD, ESCAPED_STRICT_RESERVED_WORD);
  }

  static bool IsLiteral(Value token) {
    return IsInRange(token, NULL_LITERAL, STRING);
  }

  static bool IsTemplate(Value token) {
    return IsInRange(token, TEMPLATE_SPAN, TEMPLATE_TAIL);
  }

  static bool IsMember(Value token) {
    return IsInRange(token, TEMPLATE_SPAN, LBRACK);
  }

  static bool IsProperty(Value token) {
    return IsInRange(token, PERIOD, LBRACK);
  }

  static bool IsPropertyOrCall(Value token) {
    return IsInRange(token, TEMPLATE_SPAN, LPAREN);
  }

  static bool IsArrowOrAssignmentOp(Value token) {
    return IsInRange(token, ARROW, ASSIGN_SUB);
  }

  static bool IsAssignmentOp(Value token) {
    return IsInRange(token, INIT, ASSIGN_SUB);
  }

  static bool IsBinaryOp(Value op) { return IsInRange(op, COMMA, SUB); }

  static bool IsCompareOp(Value op) { return IsInRange(op, EQ, IN); }

  static bool IsOrderedRelationalCompareOp(Value op) {
    return IsInRange(op, LT, GTE);
  }

  static bool IsEqualityOp(Value op) { return IsInRange(op, EQ, EQ_STRICT); }

  static Value BinaryOpForAssignment(Value op) {
    DCHECK(IsInRange(op, ASSIGN_BIT_OR, ASSIGN_SUB));
    Value result = static_cast<Value>(op - ASSIGN_BIT_OR + BIT_OR);
    DCHECK(IsBinaryOp(result));
    return result;
  }

  static bool IsBitOp(Value op) {
    return IsInRange(op, BIT_OR, SHR) || op == BIT_NOT;
  }

  static bool IsUnaryOp(Value op) { return IsInRange(op, ADD, VOID); }
  static bool IsCountOp(Value op) { return IsInRange(op, INC, DEC); }
  static bool IsUnaryOrCountOp(Value op) { return IsInRange(op, ADD, DEC); }
  static bool IsShiftOp(Value op) { return IsInRange(op, SHL, SHR); }

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
  static int Precedence(Value token, bool accept_IN) {
    DCHECK_GT(NUM_TOKENS, token);  // token is unsigned
    return precedence_[accept_IN][token];
  }

 private:
  static const char* const name_[NUM_TOKENS];
  static const char* const string_[NUM_TOKENS];
  static const uint8_t string_length_[NUM_TOKENS];
  static const int8_t precedence_[2][NUM_TOKENS];
  static const uint8_t token_flags[NUM_TOKENS];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_TOKEN_H_
