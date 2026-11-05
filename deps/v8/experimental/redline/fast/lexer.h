// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_LEXER_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_LEXER_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace redline {
namespace fast {

// Punctuators: V(name, text). Keywords: K(name, text).
#define FAST_PUNCTUATOR_LIST(V) \
  V(kLeftBrace, "{")            \
  V(kRightBrace, "}")           \
  V(kLeftParen, "(")            \
  V(kRightParen, ")")           \
  V(kLeftBracket, "[")          \
  V(kRightBracket, "]")         \
  V(kPeriod, ".")               \
  V(kEllipsis, "...")           \
  V(kSemicolon, ";")            \
  V(kComma, ",")                \
  V(kLessThan, "<")             \
  V(kGreaterThan, ">")          \
  V(kLessThanEq, "<=")          \
  V(kGreaterThanEq, ">=")       \
  V(kEq, "==")                  \
  V(kNotEq, "!=")               \
  V(kStrictEq, "===")           \
  V(kStrictNotEq, "!==")        \
  V(kAdd, "+")                  \
  V(kSub, "-")                  \
  V(kMul, "*")                  \
  V(kDiv, "/")                  \
  V(kMod, "%")                  \
  V(kExp, "**")                 \
  V(kInc, "++")                 \
  V(kDec, "--")                 \
  V(kShl, "<<")                 \
  V(kSar, ">>")                 \
  V(kShr, ">>>")                \
  V(kBitAnd, "&")               \
  V(kBitOr, "|")                \
  V(kBitXor, "^")               \
  V(kNot, "!")                  \
  V(kBitNot, "~")               \
  V(kAnd, "&&")                 \
  V(kOr, "||")                  \
  V(kNullish, "??")             \
  V(kQuestion, "?")             \
  V(kQuestionPeriod, "?.")      \
  V(kColon, ":")                \
  V(kAssign, "=")               \
  V(kAssignAdd, "+=")           \
  V(kAssignSub, "-=")           \
  V(kAssignMul, "*=")           \
  V(kAssignDiv, "/=")           \
  V(kAssignMod, "%=")           \
  V(kAssignExp, "**=")          \
  V(kAssignShl, "<<=")          \
  V(kAssignSar, ">>=")          \
  V(kAssignShr, ">>>=")         \
  V(kAssignBitAnd, "&=")        \
  V(kAssignBitOr, "|=")         \
  V(kAssignBitXor, "^=")        \
  V(kAssignAnd, "&&=")          \
  V(kAssignOr, "||=")           \
  V(kAssignNullish, "??=")      \
  V(kArrow, "=>")               \
  V(kAt, "@")

#define FAST_KEYWORD_LIST(K)   \
  K(kBreak, "break")           \
  K(kCase, "case")             \
  K(kCatch, "catch")           \
  K(kClass, "class")           \
  K(kConst, "const")           \
  K(kContinue, "continue")     \
  K(kDebugger, "debugger")     \
  K(kDefault, "default")       \
  K(kDelete, "delete")         \
  K(kDo, "do")                 \
  K(kElse, "else")             \
  K(kEnum, "enum")             \
  K(kExport, "export")         \
  K(kExtends, "extends")       \
  K(kFalse, "false")           \
  K(kFinally, "finally")       \
  K(kFor, "for")               \
  K(kFunction, "function")     \
  K(kIf, "if")                 \
  K(kImport, "import")         \
  K(kIn, "in")                 \
  K(kInstanceof, "instanceof") \
  K(kNew, "new")               \
  K(kNull, "null")             \
  K(kReturn, "return")         \
  K(kSuper, "super")           \
  K(kSwitch, "switch")         \
  K(kThis, "this")             \
  K(kThrow, "throw")           \
  K(kTrue, "true")             \
  K(kTry, "try")               \
  K(kTypeof, "typeof")         \
  K(kVar, "var")               \
  K(kVoid, "void")             \
  K(kWhile, "while")           \
  K(kWith, "with")

enum class Tok : uint8_t {
#define DECLARE_TOKEN(name, text) name,
  FAST_PUNCTUATOR_LIST(DECLARE_TOKEN) FAST_KEYWORD_LIST(DECLARE_TOKEN)
#undef DECLARE_TOKEN
  // Identifiers, including contextual keywords (let, yield, await, async,
  // of, get, set, static, using, ...), and escaped reserved words.
  kName,
  kPrivateName,  // #name
  kNumber,
  kBigInt,
  kString,
  kTemplateHead,            // `...${
  kNoSubstitutionTemplate,  // `...`
  kTemplateMiddle,          // }...${
  kTemplateTail,            // }...`
  kRegExp,
  kEOF,
  kIllegal,
};

const char* TokName(Tok tok);
bool IsKeyword(Tok tok);
bool IsAssignmentOperator(Tok tok);

struct Token {
  Tok type = Tok::kEOF;
  int32_t start = 0;
  int32_t end = 0;
  // A line terminator precedes the token (for ASI and restricted productions).
  bool newline_before = false;
  // Identifier/keyword written with unicode escapes.
  bool has_escape = false;
  // Numeric literal with a legacy octal / leading-zero form (strict error).
  bool legacy_octal = false;
  // Strings/templates: contained a legacy octal escape or \8 \9.
  bool legacy_octal_escape = false;
  // Templates: the cooked value is invalid (only allowed in tagged templates).
  bool cooked_valid = true;
  double number_value = 0;
  // Cooked value: identifier name (always set), string contents, template
  // span contents, regexp pattern.
  std::string value;
  // RegExp flags.
  std::string flags;
};

class Lexer {
 public:
  explicit Lexer(std::string_view source);

  // Scans the next token; '/' and '/=' are returned as division operators.
  const Token& Next();
  const Token& current() const { return token_; }

  // Called by the parser when the current token is kDiv or kAssignDiv in a
  // position where a regular expression literal is allowed.
  bool RescanAsRegExp();
  // Called by the parser when the current token is kRightBrace closing a
  // template substitution.
  bool RescanAsTemplateContinuation();

  struct State {
    size_t pos;
    Token token;
    bool at_line_start;
  };
  State Save() const { return State{pos_, token_, at_line_start_}; }
  void Restore(const State& state) {
    pos_ = state.pos;
    token_ = state.token;
    at_line_start_ = state.at_line_start;
  }

  bool has_error() const { return !error_.empty(); }
  const std::string& error() const { return error_; }
  int32_t error_pos() const { return error_pos_; }

  std::string_view source() const { return source_; }
  std::string_view TokenText(const Token& token) const {
    return source_.substr(static_cast<size_t>(token.start),
                          static_cast<size_t>(token.end - token.start));
  }

  // Number of bytes consumed so far.
  size_t position() const { return pos_; }

 private:
  // Returns the code point at pos_ and its length in bytes (0 at EOF).
  uint32_t Peek(size_t* length) const;
  uint32_t PeekAt(size_t pos, size_t* length) const;
  void SkipWhitespaceAndComments(bool* newline_seen);
  void ScanToken();
  void ScanIdentifierOrKeyword(uint32_t first, size_t first_len,
                               bool is_private);
  void ScanNumber(uint32_t first);
  void ScanString(uint32_t quote);
  void ScanTemplateSpan(bool is_head);
  bool ScanEscape(std::string* out, bool in_template, bool* cooked_valid);
  bool ScanUnicodeEscapeInIdentifier(uint32_t* code_point);
  void SetError(const std::string& message, size_t pos);
  void Finish(Tok type);

  std::string_view source_;
  size_t pos_ = 0;
  size_t token_start_ = 0;
  bool at_line_start_ = true;
  Token token_;
  std::string error_;
  int32_t error_pos_ = -1;
};

// UTF-8 helpers shared with the printer.
void AppendUtf8(std::string* out, uint32_t code_point);
// Decodes one code point at `pos` (returns U+FFFD for malformed input and
// consumes one byte).
uint32_t DecodeUtf8(std::string_view text, size_t pos, size_t* length);

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_LEXER_H_
