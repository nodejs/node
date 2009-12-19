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

#ifndef V8_TOKEN_H_
#define V8_TOKEN_H_

namespace v8 {
namespace internal {

// TOKEN_LIST takes a list of 3 macros M, all of which satisfy the
// same signature M(name, string, precedence), where name is the
// symbolic token name, string is the corresponding syntactic symbol
// (or NULL, for literals), and precedence is the precedence (or 0).
// The parameters are invoked for token categories as follows:
//
//   T: Non-keyword tokens
//   K: Keyword tokens
//   F: Future (reserved) keyword tokens

// IGNORE_TOKEN is a convenience macro that can be supplied as
// an argument (at any position) for a TOKEN_LIST call. It does
// nothing with tokens belonging to the respective category.

#define IGNORE_TOKEN(name, string, precedence)

#define TOKEN_LIST(T, K, F)                                             \
  /* End of source indicator. */                                        \
  T(EOS, "EOS", 0)                                                      \
                                                                        \
  /* Punctuators (ECMA-262, section 7.7, page 15). */                   \
  T(LPAREN, "(", 0)                                                     \
  T(RPAREN, ")", 0)                                                     \
  T(LBRACK, "[", 0)                                                     \
  T(RBRACK, "]", 0)                                                     \
  T(LBRACE, "{", 0)                                                     \
  T(RBRACE, "}", 0)                                                     \
  T(COLON, ":", 0)                                                      \
  T(SEMICOLON, ";", 0)                                                  \
  T(PERIOD, ".", 0)                                                     \
  T(CONDITIONAL, "?", 3)                                                \
  T(INC, "++", 0)                                                       \
  T(DEC, "--", 0)                                                       \
                                                                        \
  /* Assignment operators. */                                           \
  /* IsAssignmentOp() relies on this block of enum values */            \
  /* being contiguous and sorted in the same order! */                  \
  T(INIT_VAR, "=init_var", 2)  /* AST-use only. */                      \
  T(INIT_CONST, "=init_const", 2)  /* AST-use only. */                  \
  T(ASSIGN, "=", 2)                                                     \
  T(ASSIGN_BIT_OR, "|=", 2)                                             \
  T(ASSIGN_BIT_XOR, "^=", 2)                                            \
  T(ASSIGN_BIT_AND, "&=", 2)                                            \
  T(ASSIGN_SHL, "<<=", 2)                                               \
  T(ASSIGN_SAR, ">>=", 2)                                               \
  T(ASSIGN_SHR, ">>>=", 2)                                              \
  T(ASSIGN_ADD, "+=", 2)                                                \
  T(ASSIGN_SUB, "-=", 2)                                                \
  T(ASSIGN_MUL, "*=", 2)                                                \
  T(ASSIGN_DIV, "/=", 2)                                                \
  T(ASSIGN_MOD, "%=", 2)                                                \
                                                                        \
  /* Binary operators sorted by precedence. */                          \
  /* IsBinaryOp() relies on this block of enum values */                \
  /* being contiguous and sorted in the same order! */                  \
  T(COMMA, ",", 1)                                                      \
  T(OR, "||", 4)                                                        \
  T(AND, "&&", 5)                                                       \
  T(BIT_OR, "|", 6)                                                     \
  T(BIT_XOR, "^", 7)                                                    \
  T(BIT_AND, "&", 8)                                                    \
  T(SHL, "<<", 11)                                                      \
  T(SAR, ">>", 11)                                                      \
  T(SHR, ">>>", 11)                                                     \
  T(ADD, "+", 12)                                                       \
  T(SUB, "-", 12)                                                       \
  T(MUL, "*", 13)                                                       \
  T(DIV, "/", 13)                                                       \
  T(MOD, "%", 13)                                                       \
                                                                        \
  /* Compare operators sorted by precedence. */                         \
  /* IsCompareOp() relies on this block of enum values */               \
  /* being contiguous and sorted in the same order! */                  \
  T(EQ, "==", 9)                                                        \
  T(NE, "!=", 9)                                                        \
  T(EQ_STRICT, "===", 9)                                                \
  T(NE_STRICT, "!==", 9)                                                \
  T(LT, "<", 10)                                                        \
  T(GT, ">", 10)                                                        \
  T(LTE, "<=", 10)                                                      \
  T(GTE, ">=", 10)                                                      \
  K(INSTANCEOF, "instanceof", 10)                                       \
  K(IN, "in", 10)                                                       \
                                                                        \
  /* Unary operators. */                                                \
  /* IsUnaryOp() relies on this block of enum values */                 \
  /* being contiguous and sorted in the same order! */                  \
  T(NOT, "!", 0)                                                        \
  T(BIT_NOT, "~", 0)                                                    \
  K(DELETE, "delete", 0)                                                \
  K(TYPEOF, "typeof", 0)                                                \
  K(VOID, "void", 0)                                                    \
                                                                        \
  /* Keywords (ECMA-262, section 7.5.2, page 13). */                    \
  K(BREAK, "break", 0)                                                  \
  K(CASE, "case", 0)                                                    \
  K(CATCH, "catch", 0)                                                  \
  K(CONTINUE, "continue", 0)                                            \
  K(DEBUGGER, "debugger", 0)                                            \
  K(DEFAULT, "default", 0)                                              \
  /* DELETE */                                                          \
  K(DO, "do", 0)                                                        \
  K(ELSE, "else", 0)                                                    \
  K(FINALLY, "finally", 0)                                              \
  K(FOR, "for", 0)                                                      \
  K(FUNCTION, "function", 0)                                            \
  K(IF, "if", 0)                                                        \
  /* IN */                                                              \
  /* INSTANCEOF */                                                      \
  K(NEW, "new", 0)                                                      \
  K(RETURN, "return", 0)                                                \
  K(SWITCH, "switch", 0)                                                \
  K(THIS, "this", 0)                                                    \
  K(THROW, "throw", 0)                                                  \
  K(TRY, "try", 0)                                                      \
  /* TYPEOF */                                                          \
  K(VAR, "var", 0)                                                      \
  /* VOID */                                                            \
  K(WHILE, "while", 0)                                                  \
  K(WITH, "with", 0)                                                    \
                                                                        \
  /* Future reserved words (ECMA-262, section 7.5.3, page 14). */       \
  F(ABSTRACT, "abstract", 0)                                            \
  F(BOOLEAN, "boolean", 0)                                              \
  F(BYTE, "byte", 0)                                                    \
  F(CHAR, "char", 0)                                                    \
  F(CLASS, "class", 0)                                                  \
  K(CONST, "const", 0)                                                  \
  F(DOUBLE, "double", 0)                                                \
  F(ENUM, "enum", 0)                                                    \
  F(EXPORT, "export", 0)                                                \
  F(EXTENDS, "extends", 0)                                              \
  F(FINAL, "final", 0)                                                  \
  F(FLOAT, "float", 0)                                                  \
  F(GOTO, "goto", 0)                                                    \
  F(IMPLEMENTS, "implements", 0)                                        \
  F(IMPORT, "import", 0)                                                \
  F(INT, "int", 0)                                                      \
  F(INTERFACE, "interface", 0)                                          \
  F(LONG, "long", 0)                                                    \
  K(NATIVE, "native", 0)                                                \
  F(PACKAGE, "package", 0)                                              \
  F(PRIVATE, "private", 0)                                              \
  F(PROTECTED, "protected", 0)                                          \
  F(PUBLIC, "public", 0)                                                \
  F(SHORT, "short", 0)                                                  \
  F(STATIC, "static", 0)                                                \
  F(SUPER, "super", 0)                                                  \
  F(SYNCHRONIZED, "synchronized", 0)                                    \
  F(THROWS, "throws", 0)                                                \
  F(TRANSIENT, "transient", 0)                                          \
  F(VOLATILE, "volatile", 0)                                            \
                                                                        \
  /* Literals (ECMA-262, section 7.8, page 16). */                      \
  K(NULL_LITERAL, "null", 0)                                            \
  K(TRUE_LITERAL, "true", 0)                                            \
  K(FALSE_LITERAL, "false", 0)                                          \
  T(NUMBER, NULL, 0)                                                    \
  T(STRING, NULL, 0)                                                    \
                                                                        \
  /* Identifiers (not keywords or future reserved words). */            \
  T(IDENTIFIER, NULL, 0)                                                \
                                                                        \
  /* Illegal token - not able to scan. */                               \
  T(ILLEGAL, "ILLEGAL", 0)                                              \
                                                                        \
  /* Scanner-internal use only. */                                      \
  T(WHITESPACE, NULL, 0)


class Token {
 public:
  // All token values.
#define T(name, string, precedence) name,
  enum Value {
    TOKEN_LIST(T, T, IGNORE_TOKEN)
    NUM_TOKENS
  };
#undef T

#ifdef DEBUG
  // Returns a string corresponding to the C++ token name
  // (e.g. "LT" for the token LT).
  static const char* Name(Value tok) {
    ASSERT(0 <= tok && tok < NUM_TOKENS);
    return name_[tok];
  }
#endif

  // Predicates
  static bool IsAssignmentOp(Value tok) {
    return INIT_VAR <= tok && tok <= ASSIGN_MOD;
  }

  static bool IsBinaryOp(Value op) {
    return COMMA <= op && op <= MOD;
  }

  static bool IsCompareOp(Value op) {
    return EQ <= op && op <= IN;
  }

  static bool IsBitOp(Value op) {
    return (BIT_OR <= op && op <= SHR) || op == BIT_NOT;
  }

  static bool IsUnaryOp(Value op) {
    return (NOT <= op && op <= VOID) || op == ADD || op == SUB;
  }

  static bool IsCountOp(Value op) {
    return op == INC || op == DEC;
  }

  // Returns a string corresponding to the JS token string
  // (.e., "<" for the token LT) or NULL if the token doesn't
  // have a (unique) string (e.g. an IDENTIFIER).
  static const char* String(Value tok) {
    ASSERT(0 <= tok && tok < NUM_TOKENS);
    return string_[tok];
  }

  // Returns the precedence > 0 for binary and compare
  // operators; returns 0 otherwise.
  static int Precedence(Value tok) {
    ASSERT(0 <= tok && tok < NUM_TOKENS);
    return precedence_[tok];
  }

 private:
#ifdef DEBUG
  static const char* name_[NUM_TOKENS];
#endif
  static const char* string_[NUM_TOKENS];
  static int8_t precedence_[NUM_TOKENS];
};

} }  // namespace v8::internal

#endif  // V8_TOKEN_H_
