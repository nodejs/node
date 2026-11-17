// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/lexer.h"

#include <cstring>
#include <string_view>

#include "src/base/vector.h"
#include "src/numbers/conversions.h"
#include "src/strings/char-predicates-inl.h"

namespace redline {
namespace fast {

namespace {

constexpr uint32_t kEndOfInput = 0xFFFFFFFFu;

bool IsLineTerminator(uint32_t c) {
  return c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029;
}

bool IsAsciiIdentifierStart(uint32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
         c == '$';
}

bool IsAsciiIdentifierPart(uint32_t c) {
  return IsAsciiIdentifierStart(c) || (c >= '0' && c <= '9');
}

bool IsIdStart(uint32_t c) {
  if (c < 128) {
    return IsAsciiIdentifierStart(c);
  }
  if (c == kEndOfInput) {
    return false;
  }
  return v8::internal::IsIdentifierStart(static_cast<v8::base::uc32>(c));
}

bool IsIdPart(uint32_t c) {
  if (c < 128) {
    return IsAsciiIdentifierPart(c);
  }
  if (c == kEndOfInput) {
    return false;
  }
  return v8::internal::IsIdentifierPart(static_cast<v8::base::uc32>(c));
}

bool IsJsWhiteSpace(uint32_t c) {
  if (c == ' ' || c == '\t' || c == 0x0B || c == 0x0C) {
    return true;
  }
  if (c < 128 || c == kEndOfInput) {
    return false;
  }
  return v8::internal::IsWhiteSpace(static_cast<v8::base::uc32>(c));
}

int HexValue(uint32_t c) {
  if (c >= '0' && c <= '9') {
    return static_cast<int>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<int>(c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<int>(c - 'A' + 10);
  }
  return -1;
}

struct KeywordEntry {
  const char* text;
  Tok tok;
};

const KeywordEntry kKeywords[] = {
#define KEYWORD_ENTRY(name, text) {text, Tok::name},
    FAST_KEYWORD_LIST(KEYWORD_ENTRY)
#undef KEYWORD_ENTRY
};

Tok LookupKeyword(std::string_view text) {
  if (text.size() < 2 || text.size() > 10) {
    return Tok::kName;
  }
  for (const KeywordEntry& entry : kKeywords) {
    if (text == entry.text) {
      return entry.tok;
    }
  }
  return Tok::kName;
}

}  // namespace

const char* TokName(Tok tok) {
  switch (tok) {
#define TOKEN_NAME(name, text) \
  case Tok::name:              \
    return text;
    FAST_PUNCTUATOR_LIST(TOKEN_NAME)
    FAST_KEYWORD_LIST(TOKEN_NAME)
#undef TOKEN_NAME
    case Tok::kName:
      return "identifier";
    case Tok::kPrivateName:
      return "private name";
    case Tok::kNumber:
      return "number";
    case Tok::kBigInt:
      return "bigint";
    case Tok::kString:
      return "string";
    case Tok::kTemplateHead:
    case Tok::kNoSubstitutionTemplate:
    case Tok::kTemplateMiddle:
    case Tok::kTemplateTail:
      return "template";
    case Tok::kRegExp:
      return "regexp";
    case Tok::kEOF:
      return "end of input";
    case Tok::kIllegal:
      return "illegal token";
  }
  return "?";
}

bool IsKeyword(Tok tok) { return tok >= Tok::kBreak && tok <= Tok::kWith; }

bool IsAssignmentOperator(Tok tok) {
  return tok >= Tok::kAssign && tok <= Tok::kAssignNullish;
}

void AppendUtf8(std::string* out, uint32_t c) {
  if (c < 0x80) {
    out->push_back(static_cast<char>(c));
  } else if (c < 0x800) {
    out->push_back(static_cast<char>(0xC0 | (c >> 6)));
    out->push_back(static_cast<char>(0x80 | (c & 0x3F)));
  } else if (c < 0x10000) {
    // Includes lone surrogates (WTF-8).
    out->push_back(static_cast<char>(0xE0 | (c >> 12)));
    out->push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (c & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (c >> 18)));
    out->push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (c & 0x3F)));
  }
}

uint32_t DecodeUtf8(std::string_view text, size_t pos, size_t* length) {
  if (pos >= text.size()) {
    *length = 0;
    return kEndOfInput;
  }
  uint8_t b0 = static_cast<uint8_t>(text[pos]);
  if (b0 < 0x80) {
    *length = 1;
    return b0;
  }
  int extra;
  uint32_t c;
  if ((b0 & 0xE0) == 0xC0) {
    extra = 1;
    c = b0 & 0x1F;
  } else if ((b0 & 0xF0) == 0xE0) {
    extra = 2;
    c = b0 & 0x0F;
  } else if ((b0 & 0xF8) == 0xF0) {
    extra = 3;
    c = b0 & 0x07;
  } else {
    *length = 1;
    return 0xFFFD;
  }
  for (int i = 1; i <= extra; i++) {
    if (pos + static_cast<size_t>(i) >= text.size()) {
      *length = 1;
      return 0xFFFD;
    }
    uint8_t b = static_cast<uint8_t>(text[pos + static_cast<size_t>(i)]);
    if ((b & 0xC0) != 0x80) {
      *length = 1;
      return 0xFFFD;
    }
    c = (c << 6) | (b & 0x3F);
  }
  uint32_t min_code = extra == 1 ? 0x80u : extra == 2 ? 0x800u : 0x10000u;
  if (c < min_code || c > 0x10FFFFu) {
    *length = 1;
    return 0xFFFD;
  }
  *length = static_cast<size_t>(extra) + 1;
  return c;
}

Lexer::Lexer(std::string_view source) : source_(source) {
  // Skip a UTF-8 BOM.
  if (source_.size() >= 3 && static_cast<uint8_t>(source_[0]) == 0xEF &&
      static_cast<uint8_t>(source_[1]) == 0xBB &&
      static_cast<uint8_t>(source_[2]) == 0xBF) {
    pos_ = 3;
  }
  // Hashbang comment.
  if (source_.substr(pos_, 2) == "#!") {
    size_t len;
    while (pos_ < source_.size()) {
      uint32_t c = Peek(&len);
      if (IsLineTerminator(c)) {
        break;
      }
      pos_ += len;
    }
  }
}

uint32_t Lexer::Peek(size_t* length) const {
  return DecodeUtf8(source_, pos_, length);
}

uint32_t Lexer::PeekAt(size_t pos, size_t* length) const {
  return DecodeUtf8(source_, pos, length);
}

void Lexer::SetError(const std::string& message, size_t pos) {
  if (error_.empty()) {
    error_ = message;
    error_pos_ = static_cast<int32_t>(pos);
  }
}

void Lexer::SkipWhitespaceAndComments(bool* newline_seen) {
  size_t len;
  while (pos_ < source_.size()) {
    uint32_t c = Peek(&len);
    if (IsLineTerminator(c)) {
      *newline_seen = true;
      at_line_start_ = true;
      pos_ += len;
      continue;
    }
    if (IsJsWhiteSpace(c) || c == 0xA0 || c == 0xFEFF) {
      pos_ += len;
      continue;
    }
    if (c == '/') {
      size_t len2;
      uint32_t c2 = PeekAt(pos_ + 1, &len2);
      if (c2 == '/') {
        pos_ += 2;
        while (pos_ < source_.size()) {
          c = Peek(&len);
          if (IsLineTerminator(c)) {
            break;
          }
          pos_ += len;
        }
        continue;
      }
      if (c2 == '*') {
        size_t comment_start = pos_;
        pos_ += 2;
        bool closed = false;
        while (pos_ < source_.size()) {
          c = Peek(&len);
          if (c == '*' && pos_ + 1 < source_.size() &&
              source_[pos_ + 1] == '/') {
            pos_ += 2;
            closed = true;
            break;
          }
          if (IsLineTerminator(c)) {
            *newline_seen = true;
            at_line_start_ = true;
          }
          pos_ += len;
        }
        if (!closed) {
          SetError("Unterminated comment", comment_start);
        }
        continue;
      }
      return;
    }
    // Annex B HTML-like comments (script goal only).
    if (c == '<' && source_.substr(pos_, 4) == "<!--") {
      pos_ += 4;
      while (pos_ < source_.size()) {
        c = Peek(&len);
        if (IsLineTerminator(c)) {
          break;
        }
        pos_ += len;
      }
      continue;
    }
    if (c == '-' && at_line_start_ && source_.substr(pos_, 3) == "-->") {
      pos_ += 3;
      while (pos_ < source_.size()) {
        c = Peek(&len);
        if (IsLineTerminator(c)) {
          break;
        }
        pos_ += len;
      }
      continue;
    }
    return;
  }
}

const Token& Lexer::Next() {
  bool newline_seen = false;
  SkipWhitespaceAndComments(&newline_seen);
  token_ = Token();
  token_.newline_before = newline_seen;
  token_start_ = pos_;
  token_.start = static_cast<int32_t>(pos_);
  if (pos_ >= source_.size()) {
    token_.type = Tok::kEOF;
    token_.end = token_.start;
    return token_;
  }
  ScanToken();
  at_line_start_ = false;
  return token_;
}

void Lexer::Finish(Tok type) {
  token_.type = type;
  token_.end = static_cast<int32_t>(pos_);
}

void Lexer::ScanToken() {
  size_t len;
  uint32_t c = Peek(&len);
  switch (c) {
    case '{':
      pos_++;
      return Finish(Tok::kLeftBrace);
    case '}':
      pos_++;
      return Finish(Tok::kRightBrace);
    case '(':
      pos_++;
      return Finish(Tok::kLeftParen);
    case ')':
      pos_++;
      return Finish(Tok::kRightParen);
    case '[':
      pos_++;
      return Finish(Tok::kLeftBracket);
    case ']':
      pos_++;
      return Finish(Tok::kRightBracket);
    case ';':
      pos_++;
      return Finish(Tok::kSemicolon);
    case ',':
      pos_++;
      return Finish(Tok::kComma);
    case ':':
      pos_++;
      return Finish(Tok::kColon);
    case '~':
      pos_++;
      return Finish(Tok::kBitNot);
    case '@':
      pos_++;
      return Finish(Tok::kAt);
    case '.': {
      size_t l2;
      uint32_t c2 = PeekAt(pos_ + 1, &l2);
      if (c2 >= '0' && c2 <= '9') {
        return ScanNumber(c);
      }
      if (source_.substr(pos_, 3) == "...") {
        pos_ += 3;
        return Finish(Tok::kEllipsis);
      }
      pos_++;
      return Finish(Tok::kPeriod);
    }
    case '?': {
      if (source_.substr(pos_, 3) == "??=") {
        pos_ += 3;
        return Finish(Tok::kAssignNullish);
      }
      if (source_.substr(pos_, 2) == "??") {
        pos_ += 2;
        return Finish(Tok::kNullish);
      }
      if (source_.substr(pos_, 2) == "?.") {
        size_t l2;
        uint32_t c2 = PeekAt(pos_ + 2, &l2);
        if (!(c2 >= '0' && c2 <= '9')) {
          pos_ += 2;
          return Finish(Tok::kQuestionPeriod);
        }
      }
      pos_++;
      return Finish(Tok::kQuestion);
    }
    case '<':
      if (source_.substr(pos_, 3) == "<<=") {
        pos_ += 3;
        return Finish(Tok::kAssignShl);
      }
      if (source_.substr(pos_, 2) == "<<") {
        pos_ += 2;
        return Finish(Tok::kShl);
      }
      if (source_.substr(pos_, 2) == "<=") {
        pos_ += 2;
        return Finish(Tok::kLessThanEq);
      }
      pos_++;
      return Finish(Tok::kLessThan);
    case '>':
      if (source_.substr(pos_, 4) == ">>>=") {
        pos_ += 4;
        return Finish(Tok::kAssignShr);
      }
      if (source_.substr(pos_, 3) == ">>>") {
        pos_ += 3;
        return Finish(Tok::kShr);
      }
      if (source_.substr(pos_, 3) == ">>=") {
        pos_ += 3;
        return Finish(Tok::kAssignSar);
      }
      if (source_.substr(pos_, 2) == ">>") {
        pos_ += 2;
        return Finish(Tok::kSar);
      }
      if (source_.substr(pos_, 2) == ">=") {
        pos_ += 2;
        return Finish(Tok::kGreaterThanEq);
      }
      pos_++;
      return Finish(Tok::kGreaterThan);
    case '=':
      if (source_.substr(pos_, 3) == "===") {
        pos_ += 3;
        return Finish(Tok::kStrictEq);
      }
      if (source_.substr(pos_, 2) == "==") {
        pos_ += 2;
        return Finish(Tok::kEq);
      }
      if (source_.substr(pos_, 2) == "=>") {
        pos_ += 2;
        return Finish(Tok::kArrow);
      }
      pos_++;
      return Finish(Tok::kAssign);
    case '!':
      if (source_.substr(pos_, 3) == "!==") {
        pos_ += 3;
        return Finish(Tok::kStrictNotEq);
      }
      if (source_.substr(pos_, 2) == "!=") {
        pos_ += 2;
        return Finish(Tok::kNotEq);
      }
      pos_++;
      return Finish(Tok::kNot);
    case '+':
      if (source_.substr(pos_, 2) == "++") {
        pos_ += 2;
        return Finish(Tok::kInc);
      }
      if (source_.substr(pos_, 2) == "+=") {
        pos_ += 2;
        return Finish(Tok::kAssignAdd);
      }
      pos_++;
      return Finish(Tok::kAdd);
    case '-':
      if (source_.substr(pos_, 2) == "--") {
        pos_ += 2;
        return Finish(Tok::kDec);
      }
      if (source_.substr(pos_, 2) == "-=") {
        pos_ += 2;
        return Finish(Tok::kAssignSub);
      }
      pos_++;
      return Finish(Tok::kSub);
    case '*':
      if (source_.substr(pos_, 3) == "**=") {
        pos_ += 3;
        return Finish(Tok::kAssignExp);
      }
      if (source_.substr(pos_, 2) == "**") {
        pos_ += 2;
        return Finish(Tok::kExp);
      }
      if (source_.substr(pos_, 2) == "*=") {
        pos_ += 2;
        return Finish(Tok::kAssignMul);
      }
      pos_++;
      return Finish(Tok::kMul);
    case '/':
      if (source_.substr(pos_, 2) == "/=") {
        pos_ += 2;
        return Finish(Tok::kAssignDiv);
      }
      pos_++;
      return Finish(Tok::kDiv);
    case '%':
      if (source_.substr(pos_, 2) == "%=") {
        pos_ += 2;
        return Finish(Tok::kAssignMod);
      }
      pos_++;
      return Finish(Tok::kMod);
    case '&':
      if (source_.substr(pos_, 3) == "&&=") {
        pos_ += 3;
        return Finish(Tok::kAssignAnd);
      }
      if (source_.substr(pos_, 2) == "&&") {
        pos_ += 2;
        return Finish(Tok::kAnd);
      }
      if (source_.substr(pos_, 2) == "&=") {
        pos_ += 2;
        return Finish(Tok::kAssignBitAnd);
      }
      pos_++;
      return Finish(Tok::kBitAnd);
    case '|':
      if (source_.substr(pos_, 3) == "||=") {
        pos_ += 3;
        return Finish(Tok::kAssignOr);
      }
      if (source_.substr(pos_, 2) == "||") {
        pos_ += 2;
        return Finish(Tok::kOr);
      }
      if (source_.substr(pos_, 2) == "|=") {
        pos_ += 2;
        return Finish(Tok::kAssignBitOr);
      }
      pos_++;
      return Finish(Tok::kBitOr);
    case '^':
      if (source_.substr(pos_, 2) == "^=") {
        pos_ += 2;
        return Finish(Tok::kAssignBitXor);
      }
      pos_++;
      return Finish(Tok::kBitXor);
    case '"':
    case '\'':
      return ScanString(c);
    case '`':
      pos_++;
      return ScanTemplateSpan(true);
    case '#': {
      pos_++;
      size_t l2;
      uint32_t c2 = Peek(&l2);
      if (IsIdStart(c2) || c2 == '\\') {
        return ScanIdentifierOrKeyword(c2, l2, true);
      }
      SetError("Invalid or unexpected token", token_start_);
      return Finish(Tok::kIllegal);
    }
    case '\\': {
      return ScanIdentifierOrKeyword(c, len, false);
    }
    default:
      break;
  }
  if (c >= '0' && c <= '9') {
    return ScanNumber(c);
  }
  if (IsIdStart(c)) {
    return ScanIdentifierOrKeyword(c, len, false);
  }
  SetError("Invalid or unexpected token", token_start_);
  pos_ += len == 0 ? 1 : len;
  Finish(Tok::kIllegal);
}

bool Lexer::ScanUnicodeEscapeInIdentifier(uint32_t* code_point) {
  // pos_ is at the backslash.
  if (pos_ + 1 >= source_.size() || source_[pos_ + 1] != 'u') {
    return false;
  }
  size_t p = pos_ + 2;
  uint32_t value = 0;
  if (p < source_.size() && source_[p] == '{') {
    p++;
    int digits = 0;
    while (p < source_.size() && source_[p] != '}') {
      int h = HexValue(static_cast<uint8_t>(source_[p]));
      if (h < 0) {
        return false;
      }
      value = value * 16 + static_cast<uint32_t>(h);
      if (value > 0x10FFFF) {
        return false;
      }
      digits++;
      p++;
    }
    if (p >= source_.size() || digits == 0) {
      return false;
    }
    p++;  // '}'
  } else {
    for (int i = 0; i < 4; i++) {
      if (p >= source_.size()) {
        return false;
      }
      int h = HexValue(static_cast<uint8_t>(source_[p]));
      if (h < 0) {
        return false;
      }
      value = value * 16 + static_cast<uint32_t>(h);
      p++;
    }
  }
  pos_ = p;
  *code_point = value;
  return true;
}

void Lexer::ScanIdentifierOrKeyword(uint32_t first, size_t first_len,
                                    bool is_private) {
  std::string name;
  bool has_escape = false;
  bool is_first = true;
  (void)first;
  (void)first_len;
  while (pos_ < source_.size()) {
    size_t len;
    uint32_t c = Peek(&len);
    if (c == '\\') {
      uint32_t code_point;
      if (!ScanUnicodeEscapeInIdentifier(&code_point) ||
          !(is_first ? IsIdStart(code_point) : IsIdPart(code_point))) {
        SetError("Invalid Unicode escape sequence", pos_);
        pos_++;
        return Finish(Tok::kIllegal);
      }
      has_escape = true;
      AppendUtf8(&name, code_point);
    } else if (is_first ? IsIdStart(c) : IsIdPart(c)) {
      name.append(source_.substr(pos_, len));
      pos_ += len;
    } else {
      break;
    }
    is_first = false;
  }
  token_.has_escape = has_escape;
  token_.value = std::move(name);
  if (is_private) {
    return Finish(Tok::kPrivateName);
  }
  Tok keyword = has_escape ? Tok::kName : LookupKeyword(token_.value);
  Finish(keyword);
}

void Lexer::ScanNumber(uint32_t first) {
  size_t start = pos_;
  std::string digits;  // cleaned text for conversion
  auto is_digit = [](uint32_t c) { return c >= '0' && c <= '9'; };
  auto scan_digits = [&](auto predicate, bool allow_separator) -> bool {
    bool any = false;
    bool last_was_separator = false;
    while (pos_ < source_.size()) {
      uint8_t c = static_cast<uint8_t>(source_[pos_]);
      if (predicate(c)) {
        digits.push_back(static_cast<char>(c));
        any = true;
        last_was_separator = false;
        pos_++;
      } else if (c == '_' && allow_separator && any && !last_was_separator) {
        last_was_separator = true;
        pos_++;
      } else {
        break;
      }
    }
    if (last_was_separator) {
      SetError(
          "Numeric separators are not allowed at the end of numeric "
          "literals",
          pos_ - 1);
      return false;
    }
    return any;
  };

  auto check_no_trailing_id_or_digit = [&]() -> bool {
    size_t len;
    uint32_t next = Peek(&len);
    if (IsIdStart(next) || (next >= '0' && next <= '9')) {
      SetError("Invalid or unexpected token", pos_);
      Finish(Tok::kIllegal);
      return false;
    }
    return true;
  };

  bool is_bigint_allowed = true;
  if (first == '0' && pos_ + 1 < source_.size()) {
    char p = source_[pos_ + 1];
    int radix = 0;
    if (p == 'x' || p == 'X') {
      radix = 16;
    }
    if (p == 'o' || p == 'O') {
      radix = 8;
    }
    if (p == 'b' || p == 'B') {
      radix = 2;
    }
    if (radix != 0) {
      digits.append(source_.substr(pos_, 2));
      pos_ += 2;
      bool ok = scan_digits(
          [radix](uint8_t c) {
            int h = HexValue(c);
            return h >= 0 && h < radix;
          },
          true);
      if (!ok) {
        SetError("Invalid or unexpected token", start);
        return Finish(Tok::kIllegal);
      }
      if (pos_ < source_.size() && source_[pos_] == 'n') {
        pos_++;
        if (!check_no_trailing_id_or_digit()) {
          return;
        }
        token_.value = digits;
        return Finish(Tok::kBigInt);
      }
      if (!check_no_trailing_id_or_digit()) {
        return;
      }
      // The const char* overload is the exported one (component builds).
      token_.number_value = v8::internal::StringToDouble(
          digits.c_str(), v8::internal::ALLOW_NON_DECIMAL_PREFIX);
      return Finish(Tok::kNumber);
    }
    if (p == '_') {
      SetError("Numeric separator can not be used after leading 0", pos_ + 1);
      return Finish(Tok::kIllegal);
    }
    if (is_digit(static_cast<uint8_t>(p))) {
      // Legacy octal (017) or legacy decimal with leading zero (089).
      pos_++;
      bool octal = true;
      std::string legacy;
      while (pos_ < source_.size() &&
             is_digit(static_cast<uint8_t>(source_[pos_]))) {
        if (source_[pos_] >= '8') {
          octal = false;
        }
        legacy.push_back(source_[pos_]);
        pos_++;
      }
      if (pos_ < source_.size() && source_[pos_] == '_') {
        SetError("Numeric separator can not be used after leading 0", pos_);
        return Finish(Tok::kIllegal);
      }
      token_.legacy_octal = true;
      is_bigint_allowed = false;
      if (octal) {
        if (!check_no_trailing_id_or_digit()) {
          return;
        }
        double value = 0;
        for (char d : legacy) {
          value = value * 8 + (d - '0');
        }
        token_.number_value = value;
        return Finish(Tok::kNumber);
      }
      // Legacy decimal: may continue with a fraction/exponent.
      digits = legacy;
    }
  }
  if (digits.empty() && first != '.') {
    scan_digits(is_digit, true);
  }
  bool integer_only = true;
  if (pos_ < source_.size() && source_[pos_] == '.') {
    integer_only = false;
    digits.push_back('.');
    pos_++;
    scan_digits(is_digit, true);
  }
  if (pos_ < source_.size() && (source_[pos_] == 'e' || source_[pos_] == 'E')) {
    size_t save = pos_;
    std::string exponent = "e";
    pos_++;
    if (pos_ < source_.size() &&
        (source_[pos_] == '+' || source_[pos_] == '-')) {
      exponent.push_back(source_[pos_]);
      pos_++;
    }
    std::string saved_digits = digits;
    digits.clear();
    if (!scan_digits(is_digit, true)) {
      SetError("Invalid or unexpected token", save);
      return Finish(Tok::kIllegal);
    }
    exponent += digits;
    digits = saved_digits + exponent;
    integer_only = false;
  }
  if (integer_only && is_bigint_allowed && pos_ < source_.size() &&
      source_[pos_] == 'n') {
    pos_++;
    if (!check_no_trailing_id_or_digit()) {
      return;
    }
    token_.value = digits;
    return Finish(Tok::kBigInt);
  }
  if (digits.empty() || digits == ".") {
    SetError("Invalid or unexpected token", start);
    return Finish(Tok::kIllegal);
  }
  if (!check_no_trailing_id_or_digit()) {
    return;
  }
  // The const char* overload is the exported one (component builds).
  token_.number_value = v8::internal::StringToDouble(
      digits.c_str(), v8::internal::NO_CONVERSION_FLAG);
  Finish(Tok::kNumber);
}

// Scans an escape sequence; pos_ is at the backslash. Appends the cooked
// value to `out`. In templates, invalid escapes clear *cooked_valid instead of
// raising an error.
bool Lexer::ScanEscape(std::string* out, bool in_template, bool* cooked_valid) {
  size_t escape_start = pos_;
  pos_++;  // backslash
  size_t len;
  uint32_t c = Peek(&len);
  auto invalid = [&](const char* message) {
    if (in_template) {
      *cooked_valid = false;
      return true;
    }
    SetError(message, escape_start);
    return false;
  };
  if (c == kEndOfInput) {
    SetError("Invalid or unexpected token", escape_start);
    return false;
  }
  pos_ += len;
  switch (c) {
    case 'n':
      out->push_back('\n');
      return true;
    case 't':
      out->push_back('\t');
      return true;
    case 'r':
      out->push_back('\r');
      return true;
    case 'b':
      out->push_back('\b');
      return true;
    case 'f':
      out->push_back('\f');
      return true;
    case 'v':
      out->push_back('\v');
      return true;
    case '\r':
      // Line continuation; \r\n counts as one terminator.
      if (pos_ < source_.size() && source_[pos_] == '\n') {
        pos_++;
      }
      return true;
    case '\n':
    case 0x2028:
    case 0x2029:
      return true;
    case 'x': {
      int h1 = pos_ < source_.size()
                   ? HexValue(static_cast<uint8_t>(source_[pos_]))
                   : -1;
      int h2 = pos_ + 1 < source_.size()
                   ? HexValue(static_cast<uint8_t>(source_[pos_ + 1]))
                   : -1;
      if (h1 < 0 || h2 < 0) {
        return invalid("Invalid hexadecimal escape sequence");
      }
      pos_ += 2;
      AppendUtf8(out, static_cast<uint32_t>(h1 * 16 + h2));
      return true;
    }
    case 'u': {
      uint32_t value = 0;
      if (pos_ < source_.size() && source_[pos_] == '{') {
        pos_++;
        int digits = 0;
        while (pos_ < source_.size() && source_[pos_] != '}') {
          int h = HexValue(static_cast<uint8_t>(source_[pos_]));
          if (h < 0) {
            return invalid("Invalid Unicode escape sequence");
          }
          value = value * 16 + static_cast<uint32_t>(h);
          if (value > 0x10FFFF) {
            return invalid("Undefined Unicode code-point");
          }
          digits++;
          pos_++;
        }
        if (pos_ >= source_.size() || digits == 0) {
          return invalid("Invalid Unicode escape sequence");
        }
        pos_++;
      } else {
        for (int i = 0; i < 4; i++) {
          int h = pos_ < source_.size()
                      ? HexValue(static_cast<uint8_t>(source_[pos_]))
                      : -1;
          if (h < 0) {
            return invalid("Invalid Unicode escape sequence");
          }
          value = value * 16 + static_cast<uint32_t>(h);
          pos_++;
        }
        // Combine surrogate pairs written as two escapes.
        if (value >= 0xD800 && value <= 0xDBFF && pos_ + 5 < source_.size() &&
            source_[pos_] == '\\' && source_[pos_ + 1] == 'u') {
          uint32_t low = 0;
          bool ok = true;
          for (int i = 0; i < 4; i++) {
            int h = HexValue(static_cast<uint8_t>(
                source_[pos_ + 2 + static_cast<size_t>(i)]));
            if (h < 0) {
              ok = false;
              break;
            }
            low = low * 16 + static_cast<uint32_t>(h);
          }
          if (ok && low >= 0xDC00 && low <= 0xDFFF) {
            pos_ += 6;
            value = 0x10000 + ((value - 0xD800) << 10) + (low - 0xDC00);
          }
        }
      }
      AppendUtf8(out, value);
      return true;
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
      // \0 not followed by a digit is the null character; everything else is
      // a legacy octal escape (sloppy mode only, never in templates).
      bool next_is_digit =
          pos_ < source_.size() && source_[pos_] >= '0' && source_[pos_] <= '9';
      if (c == '0' && !next_is_digit) {
        out->push_back('\0');
        return true;
      }
      if (in_template) {
        *cooked_valid = false;
        return true;
      }
      token_.legacy_octal_escape = true;
      uint32_t value = c - '0';
      // Up to two more octal digits (three total, max \377).
      int max_more = c <= '3' ? 2 : 1;
      for (int i = 0; i < max_more; i++) {
        if (pos_ < source_.size() && source_[pos_] >= '0' &&
            source_[pos_] <= '7') {
          value = value * 8 + static_cast<uint32_t>(source_[pos_] - '0');
          pos_++;
        } else {
          break;
        }
      }
      AppendUtf8(out, value);
      return true;
    }
    case '8':
    case '9':
      if (in_template) {
        *cooked_valid = false;
        return true;
      }
      token_.legacy_octal_escape = true;
      out->push_back(static_cast<char>(c));
      return true;
    default:
      AppendUtf8(out, c);
      return true;
  }
}

void Lexer::ScanString(uint32_t quote) {
  pos_++;  // opening quote
  std::string value;
  while (true) {
    size_t len;
    uint32_t c = Peek(&len);
    if (c == kEndOfInput || c == '\n' || c == '\r') {
      SetError("Invalid or unexpected token", token_start_);
      return Finish(Tok::kIllegal);
    }
    if (c == quote) {
      pos_++;
      break;
    }
    if (c == '\\') {
      bool cooked_valid = true;
      if (!ScanEscape(&value, false, &cooked_valid)) {
        return Finish(Tok::kIllegal);
      }
      continue;
    }
    value.append(source_.substr(pos_, len));
    pos_ += len;
  }
  token_.value = std::move(value);
  Finish(Tok::kString);
}

// Scans a template span; pos_ is just after the opening '`' or the '}' that
// closes a substitution.
void Lexer::ScanTemplateSpan(bool is_head) {
  std::string cooked;
  bool cooked_valid = true;
  while (true) {
    size_t len;
    uint32_t c = Peek(&len);
    if (c == kEndOfInput) {
      SetError("Unterminated template literal", token_start_);
      return Finish(Tok::kIllegal);
    }
    if (c == '`') {
      pos_++;
      token_.value = std::move(cooked);
      token_.cooked_valid = cooked_valid;
      return Finish(is_head ? Tok::kNoSubstitutionTemplate
                            : Tok::kTemplateTail);
    }
    if (c == '$' && pos_ + 1 < source_.size() && source_[pos_ + 1] == '{') {
      pos_ += 2;
      token_.value = std::move(cooked);
      token_.cooked_valid = cooked_valid;
      return Finish(is_head ? Tok::kTemplateHead : Tok::kTemplateMiddle);
    }
    if (c == '\\') {
      if (!ScanEscape(&cooked, true, &cooked_valid)) {
        return Finish(Tok::kIllegal);
      }
      continue;
    }
    if (c == '\r') {
      // Normalize CRLF / CR to LF in the cooked value.
      pos_++;
      if (pos_ < source_.size() && source_[pos_] == '\n') {
        pos_++;
      }
      cooked.push_back('\n');
      continue;
    }
    cooked.append(source_.substr(pos_, len));
    pos_ += len;
  }
}

bool Lexer::RescanAsTemplateContinuation() {
  if (token_.type != Tok::kRightBrace) {
    return false;
  }
  pos_ = static_cast<size_t>(token_.start) + 1;
  token_start_ = static_cast<size_t>(token_.start);
  bool newline_before = token_.newline_before;
  token_ = Token();
  token_.newline_before = newline_before;
  token_.start = static_cast<int32_t>(token_start_);
  ScanTemplateSpan(false);
  return token_.type != Tok::kIllegal;
}

bool Lexer::RescanAsRegExp() {
  if (token_.type != Tok::kDiv && token_.type != Tok::kAssignDiv) {
    return false;
  }
  pos_ = static_cast<size_t>(token_.start) + 1;
  token_start_ = static_cast<size_t>(token_.start);
  bool newline_before = token_.newline_before;
  token_ = Token();
  token_.newline_before = newline_before;
  token_.start = static_cast<int32_t>(token_start_);
  std::string pattern;
  bool in_class = false;
  while (true) {
    size_t len;
    uint32_t c = Peek(&len);
    if (c == kEndOfInput || IsLineTerminator(c)) {
      SetError("Invalid regular expression: missing /", token_start_);
      Finish(Tok::kIllegal);
      return false;
    }
    if (c == '\\') {
      pattern.push_back('\\');
      pos_++;
      c = Peek(&len);
      if (c == kEndOfInput || IsLineTerminator(c)) {
        SetError("Invalid regular expression: missing /", token_start_);
        Finish(Tok::kIllegal);
        return false;
      }
      pattern.append(source_.substr(pos_, len));
      pos_ += len;
      continue;
    }
    if (c == '[') {
      in_class = true;
    } else if (c == ']') {
      in_class = false;
    } else if (c == '/' && !in_class) {
      pos_++;
      break;
    }
    pattern.append(source_.substr(pos_, len));
    pos_ += len;
  }
  std::string flags;
  while (pos_ < source_.size()) {
    size_t len;
    uint32_t c = Peek(&len);
    if (!IsIdPart(c)) {
      break;
    }
    flags.append(source_.substr(pos_, len));
    pos_ += len;
  }
  token_.value = std::move(pattern);
  token_.flags = std::move(flags);
  Finish(Tok::kRegExp);
  return true;
}

}  // namespace fast
}  // namespace redline
