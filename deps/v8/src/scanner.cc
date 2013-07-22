// Copyright 2011 the V8 project authors. All rights reserved.
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

// Features shared by parsing and pre-parsing scanners.

#include "scanner.h"

#include "../include/v8stdint.h"
#include "char-predicates-inl.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Scanner

Scanner::Scanner(UnicodeCache* unicode_cache)
    : unicode_cache_(unicode_cache),
      octal_pos_(Location::invalid()),
      harmony_scoping_(false),
      harmony_modules_(false),
      harmony_numeric_literals_(false) { }


void Scanner::Initialize(Utf16CharacterStream* source) {
  source_ = source;
  // Need to capture identifiers in order to recognize "get" and "set"
  // in object literals.
  Init();
  // Skip initial whitespace allowing HTML comment ends just like
  // after a newline and scan first token.
  has_line_terminator_before_next_ = true;
  SkipWhiteSpace();
  Scan();
}


uc32 Scanner::ScanHexNumber(int expected_length) {
  ASSERT(expected_length <= 4);  // prevent overflow

  uc32 digits[4] = { 0, 0, 0, 0 };
  uc32 x = 0;
  for (int i = 0; i < expected_length; i++) {
    digits[i] = c0_;
    int d = HexValue(c0_);
    if (d < 0) {
      // According to ECMA-262, 3rd, 7.8.4, page 18, these hex escapes
      // should be illegal, but other JS VMs just return the
      // non-escaped version of the original character.

      // Push back digits that we have advanced past.
      for (int j = i-1; j >= 0; j--) {
        PushBack(digits[j]);
      }
      return -1;
    }
    x = x * 16 + d;
    Advance();
  }

  return x;
}


// Ensure that tokens can be stored in a byte.
STATIC_ASSERT(Token::NUM_TOKENS <= 0x100);

// Table of one-character tokens, by character (0x00..0x7f only).
static const byte one_char_tokens[] = {
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::LPAREN,       // 0x28
  Token::RPAREN,       // 0x29
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::COMMA,        // 0x2c
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::COLON,        // 0x3a
  Token::SEMICOLON,    // 0x3b
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::CONDITIONAL,  // 0x3f
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::LBRACK,     // 0x5b
  Token::ILLEGAL,
  Token::RBRACK,     // 0x5d
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::LBRACE,       // 0x7b
  Token::ILLEGAL,
  Token::RBRACE,       // 0x7d
  Token::BIT_NOT,      // 0x7e
  Token::ILLEGAL
};


Token::Value Scanner::Next() {
  current_ = next_;
  has_line_terminator_before_next_ = false;
  has_multiline_comment_before_next_ = false;
  if (static_cast<unsigned>(c0_) <= 0x7f) {
    Token::Value token = static_cast<Token::Value>(one_char_tokens[c0_]);
    if (token != Token::ILLEGAL) {
      int pos = source_pos();
      next_.token = token;
      next_.location.beg_pos = pos;
      next_.location.end_pos = pos + 1;
      Advance();
      return current_.token;
    }
  }
  Scan();
  return current_.token;
}


static inline bool IsByteOrderMark(uc32 c) {
  // The Unicode value U+FFFE is guaranteed never to be assigned as a
  // Unicode character; this implies that in a Unicode context the
  // 0xFF, 0xFE byte pattern can only be interpreted as the U+FEFF
  // character expressed in little-endian byte order (since it could
  // not be a U+FFFE character expressed in big-endian byte
  // order). Nevertheless, we check for it to be compatible with
  // Spidermonkey.
  return c == 0xFEFF || c == 0xFFFE;
}


bool Scanner::SkipWhiteSpace() {
  int start_position = source_pos();

  while (true) {
    // We treat byte-order marks (BOMs) as whitespace for better
    // compatibility with Spidermonkey and other JavaScript engines.
    while (unicode_cache_->IsWhiteSpace(c0_) || IsByteOrderMark(c0_)) {
      // IsWhiteSpace() includes line terminators!
      if (unicode_cache_->IsLineTerminator(c0_)) {
        // Ignore line terminators, but remember them. This is necessary
        // for automatic semicolon insertion.
        has_line_terminator_before_next_ = true;
      }
      Advance();
    }

    // If there is an HTML comment end '-->' at the beginning of a
    // line (with only whitespace in front of it), we treat the rest
    // of the line as a comment. This is in line with the way
    // SpiderMonkey handles it.
    if (c0_ == '-' && has_line_terminator_before_next_) {
      Advance();
      if (c0_ == '-') {
        Advance();
        if (c0_ == '>') {
          // Treat the rest of the line as a comment.
          SkipSingleLineComment();
          // Continue skipping white space after the comment.
          continue;
        }
        PushBack('-');  // undo Advance()
      }
      PushBack('-');  // undo Advance()
    }
    // Return whether or not we skipped any characters.
    return source_pos() != start_position;
  }
}


Token::Value Scanner::SkipSingleLineComment() {
  Advance();

  // The line terminator at the end of the line is not considered
  // to be part of the single-line comment; it is recognized
  // separately by the lexical grammar and becomes part of the
  // stream of input elements for the syntactic grammar (see
  // ECMA-262, section 7.4).
  while (c0_ >= 0 && !unicode_cache_->IsLineTerminator(c0_)) {
    Advance();
  }

  return Token::WHITESPACE;
}


Token::Value Scanner::SkipMultiLineComment() {
  ASSERT(c0_ == '*');
  Advance();

  while (c0_ >= 0) {
    uc32 ch = c0_;
    Advance();
    if (unicode_cache_->IsLineTerminator(ch)) {
      // Following ECMA-262, section 7.4, a comment containing
      // a newline will make the comment count as a line-terminator.
      has_multiline_comment_before_next_ = true;
    }
    // If we have reached the end of the multi-line comment, we
    // consume the '/' and insert a whitespace. This way all
    // multi-line comments are treated as whitespace.
    if (ch == '*' && c0_ == '/') {
      c0_ = ' ';
      return Token::WHITESPACE;
    }
  }

  // Unterminated multi-line comment.
  return Token::ILLEGAL;
}


Token::Value Scanner::ScanHtmlComment() {
  // Check for <!-- comments.
  ASSERT(c0_ == '!');
  Advance();
  if (c0_ == '-') {
    Advance();
    if (c0_ == '-') return SkipSingleLineComment();
    PushBack('-');  // undo Advance()
  }
  PushBack('!');  // undo Advance()
  ASSERT(c0_ == '!');
  return Token::LT;
}


void Scanner::Scan() {
  next_.literal_chars = NULL;
  Token::Value token;
  do {
    // Remember the position of the next token
    next_.location.beg_pos = source_pos();

    switch (c0_) {
      case ' ':
      case '\t':
        Advance();
        token = Token::WHITESPACE;
        break;

      case '\n':
        Advance();
        has_line_terminator_before_next_ = true;
        token = Token::WHITESPACE;
        break;

      case '"': case '\'':
        token = ScanString();
        break;

      case '<':
        // < <= << <<= <!--
        Advance();
        if (c0_ == '=') {
          token = Select(Token::LTE);
        } else if (c0_ == '<') {
          token = Select('=', Token::ASSIGN_SHL, Token::SHL);
        } else if (c0_ == '!') {
          token = ScanHtmlComment();
        } else {
          token = Token::LT;
        }
        break;

      case '>':
        // > >= >> >>= >>> >>>=
        Advance();
        if (c0_ == '=') {
          token = Select(Token::GTE);
        } else if (c0_ == '>') {
          // >> >>= >>> >>>=
          Advance();
          if (c0_ == '=') {
            token = Select(Token::ASSIGN_SAR);
          } else if (c0_ == '>') {
            token = Select('=', Token::ASSIGN_SHR, Token::SHR);
          } else {
            token = Token::SAR;
          }
        } else {
          token = Token::GT;
        }
        break;

      case '=':
        // = == ===
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::EQ_STRICT, Token::EQ);
        } else {
          token = Token::ASSIGN;
        }
        break;

      case '!':
        // ! != !==
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::NE_STRICT, Token::NE);
        } else {
          token = Token::NOT;
        }
        break;

      case '+':
        // + ++ +=
        Advance();
        if (c0_ == '+') {
          token = Select(Token::INC);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_ADD);
        } else {
          token = Token::ADD;
        }
        break;

      case '-':
        // - -- --> -=
        Advance();
        if (c0_ == '-') {
          Advance();
          if (c0_ == '>' && has_line_terminator_before_next_) {
            // For compatibility with SpiderMonkey, we skip lines that
            // start with an HTML comment end '-->'.
            token = SkipSingleLineComment();
          } else {
            token = Token::DEC;
          }
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_SUB);
        } else {
          token = Token::SUB;
        }
        break;

      case '*':
        // * *=
        token = Select('=', Token::ASSIGN_MUL, Token::MUL);
        break;

      case '%':
        // % %=
        token = Select('=', Token::ASSIGN_MOD, Token::MOD);
        break;

      case '/':
        // /  // /* /=
        Advance();
        if (c0_ == '/') {
          token = SkipSingleLineComment();
        } else if (c0_ == '*') {
          token = SkipMultiLineComment();
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_DIV);
        } else {
          token = Token::DIV;
        }
        break;

      case '&':
        // & && &=
        Advance();
        if (c0_ == '&') {
          token = Select(Token::AND);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_BIT_AND);
        } else {
          token = Token::BIT_AND;
        }
        break;

      case '|':
        // | || |=
        Advance();
        if (c0_ == '|') {
          token = Select(Token::OR);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_BIT_OR);
        } else {
          token = Token::BIT_OR;
        }
        break;

      case '^':
        // ^ ^=
        token = Select('=', Token::ASSIGN_BIT_XOR, Token::BIT_XOR);
        break;

      case '.':
        // . Number
        Advance();
        if (IsDecimalDigit(c0_)) {
          token = ScanNumber(true);
        } else {
          token = Token::PERIOD;
        }
        break;

      case ':':
        token = Select(Token::COLON);
        break;

      case ';':
        token = Select(Token::SEMICOLON);
        break;

      case ',':
        token = Select(Token::COMMA);
        break;

      case '(':
        token = Select(Token::LPAREN);
        break;

      case ')':
        token = Select(Token::RPAREN);
        break;

      case '[':
        token = Select(Token::LBRACK);
        break;

      case ']':
        token = Select(Token::RBRACK);
        break;

      case '{':
        token = Select(Token::LBRACE);
        break;

      case '}':
        token = Select(Token::RBRACE);
        break;

      case '?':
        token = Select(Token::CONDITIONAL);
        break;

      case '~':
        token = Select(Token::BIT_NOT);
        break;

      default:
        if (unicode_cache_->IsIdentifierStart(c0_)) {
          token = ScanIdentifierOrKeyword();
        } else if (IsDecimalDigit(c0_)) {
          token = ScanNumber(false);
        } else if (SkipWhiteSpace()) {
          token = Token::WHITESPACE;
        } else if (c0_ < 0) {
          token = Token::EOS;
        } else {
          token = Select(Token::ILLEGAL);
        }
        break;
    }

    // Continue scanning for tokens as long as we're just skipping
    // whitespace.
  } while (token == Token::WHITESPACE);

  next_.location.end_pos = source_pos();
  next_.token = token;
}


void Scanner::SeekForward(int pos) {
  // After this call, we will have the token at the given position as
  // the "next" token. The "current" token will be invalid.
  if (pos == next_.location.beg_pos) return;
  int current_pos = source_pos();
  ASSERT_EQ(next_.location.end_pos, current_pos);
  // Positions inside the lookahead token aren't supported.
  ASSERT(pos >= current_pos);
  if (pos != current_pos) {
    source_->SeekForward(pos - source_->pos());
    Advance();
    // This function is only called to seek to the location
    // of the end of a function (at the "}" token). It doesn't matter
    // whether there was a line terminator in the part we skip.
    has_line_terminator_before_next_ = false;
    has_multiline_comment_before_next_ = false;
  }
  Scan();
}


bool Scanner::ScanEscape() {
  uc32 c = c0_;
  Advance();

  // Skip escaped newlines.
  if (unicode_cache_->IsLineTerminator(c)) {
    // Allow CR+LF newlines in multiline string literals.
    if (IsCarriageReturn(c) && IsLineFeed(c0_)) Advance();
    // Allow LF+CR newlines in multiline string literals.
    if (IsLineFeed(c) && IsCarriageReturn(c0_)) Advance();
    return true;
  }

  switch (c) {
    case '\'':  // fall through
    case '"' :  // fall through
    case '\\': break;
    case 'b' : c = '\b'; break;
    case 'f' : c = '\f'; break;
    case 'n' : c = '\n'; break;
    case 'r' : c = '\r'; break;
    case 't' : c = '\t'; break;
    case 'u' : {
      c = ScanHexNumber(4);
      if (c < 0) return false;
      break;
    }
    case 'v' : c = '\v'; break;
    case 'x' : {
      c = ScanHexNumber(2);
      if (c < 0) return false;
      break;
    }
    case '0' :  // fall through
    case '1' :  // fall through
    case '2' :  // fall through
    case '3' :  // fall through
    case '4' :  // fall through
    case '5' :  // fall through
    case '6' :  // fall through
    case '7' : c = ScanOctalEscape(c, 2); break;
  }

  // According to ECMA-262, section 7.8.4, characters not covered by the
  // above cases should be illegal, but they are commonly handled as
  // non-escaped characters by JS VMs.
  AddLiteralChar(c);
  return true;
}


// Octal escapes of the forms '\0xx' and '\xxx' are not a part of
// ECMA-262. Other JS VMs support them.
uc32 Scanner::ScanOctalEscape(uc32 c, int length) {
  uc32 x = c - '0';
  int i = 0;
  for (; i < length; i++) {
    int d = c0_ - '0';
    if (d < 0 || d > 7) break;
    int nx = x * 8 + d;
    if (nx >= 256) break;
    x = nx;
    Advance();
  }
  // Anything except '\0' is an octal escape sequence, illegal in strict mode.
  // Remember the position of octal escape sequences so that an error
  // can be reported later (in strict mode).
  // We don't report the error immediately, because the octal escape can
  // occur before the "use strict" directive.
  if (c != '0' || i > 0) {
    octal_pos_ = Location(source_pos() - i - 1, source_pos() - 1);
  }
  return x;
}


Token::Value Scanner::ScanString() {
  uc32 quote = c0_;
  Advance();  // consume quote

  LiteralScope literal(this);
  while (c0_ != quote && c0_ >= 0
         && !unicode_cache_->IsLineTerminator(c0_)) {
    uc32 c = c0_;
    Advance();
    if (c == '\\') {
      if (c0_ < 0 || !ScanEscape()) return Token::ILLEGAL;
    } else {
      AddLiteralChar(c);
    }
  }
  if (c0_ != quote) return Token::ILLEGAL;
  literal.Complete();

  Advance();  // consume quote
  return Token::STRING;
}


void Scanner::ScanDecimalDigits() {
  while (IsDecimalDigit(c0_))
    AddLiteralCharAdvance();
}


Token::Value Scanner::ScanNumber(bool seen_period) {
  ASSERT(IsDecimalDigit(c0_));  // the first digit of the number or the fraction

  enum { DECIMAL, HEX, OCTAL, IMPLICIT_OCTAL, BINARY } kind = DECIMAL;

  LiteralScope literal(this);
  if (seen_period) {
    // we have already seen a decimal point of the float
    AddLiteralChar('.');
    ScanDecimalDigits();  // we know we have at least one digit

  } else {
    // if the first character is '0' we must check for octals and hex
    if (c0_ == '0') {
      int start_pos = source_pos();  // For reporting octal positions.
      AddLiteralCharAdvance();

      // either 0, 0exxx, 0Exxx, 0.xxx, a hex number, a binary number or
      // an octal number.
      if (c0_ == 'x' || c0_ == 'X') {
        // hex number
        kind = HEX;
        AddLiteralCharAdvance();
        if (!IsHexDigit(c0_)) {
          // we must have at least one hex digit after 'x'/'X'
          return Token::ILLEGAL;
        }
        while (IsHexDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if (harmony_numeric_literals_ && (c0_ == 'o' || c0_ == 'O')) {
        kind = OCTAL;
        AddLiteralCharAdvance();
        if (!IsOctalDigit(c0_)) {
          // we must have at least one octal digit after 'o'/'O'
          return Token::ILLEGAL;
        }
        while (IsOctalDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if (harmony_numeric_literals_ && (c0_ == 'b' || c0_ == 'B')) {
        kind = BINARY;
        AddLiteralCharAdvance();
        if (!IsBinaryDigit(c0_)) {
          // we must have at least one binary digit after 'b'/'B'
          return Token::ILLEGAL;
        }
        while (IsBinaryDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if ('0' <= c0_ && c0_ <= '7') {
        // (possible) octal number
        kind = IMPLICIT_OCTAL;
        while (true) {
          if (c0_ == '8' || c0_ == '9') {
            kind = DECIMAL;
            break;
          }
          if (c0_  < '0' || '7'  < c0_) {
            // Octal literal finished.
            octal_pos_ = Location(start_pos, source_pos());
            break;
          }
          AddLiteralCharAdvance();
        }
      }
    }

    // Parse decimal digits and allow trailing fractional part.
    if (kind == DECIMAL) {
      ScanDecimalDigits();  // optional
      if (c0_ == '.') {
        AddLiteralCharAdvance();
        ScanDecimalDigits();  // optional
      }
    }
  }

  // scan exponent, if any
  if (c0_ == 'e' || c0_ == 'E') {
    ASSERT(kind != HEX);  // 'e'/'E' must be scanned as part of the hex number
    if (kind != DECIMAL) return Token::ILLEGAL;
    // scan exponent
    AddLiteralCharAdvance();
    if (c0_ == '+' || c0_ == '-')
      AddLiteralCharAdvance();
    if (!IsDecimalDigit(c0_)) {
      // we must have at least one decimal digit after 'e'/'E'
      return Token::ILLEGAL;
    }
    ScanDecimalDigits();
  }

  // The source character immediately following a numeric literal must
  // not be an identifier start or a decimal digit; see ECMA-262
  // section 7.8.3, page 17 (note that we read only one decimal digit
  // if the value is 0).
  if (IsDecimalDigit(c0_) || unicode_cache_->IsIdentifierStart(c0_))
    return Token::ILLEGAL;

  literal.Complete();

  return Token::NUMBER;
}


uc32 Scanner::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return -1;
  Advance();
  uc32 result = ScanHexNumber(4);
  if (result < 0) PushBack('u');
  return result;
}


// ----------------------------------------------------------------------------
// Keyword Matcher

#define KEYWORDS(KEYWORD_GROUP, KEYWORD)                            \
  KEYWORD_GROUP('b')                                                \
  KEYWORD("break", Token::BREAK)                                    \
  KEYWORD_GROUP('c')                                                \
  KEYWORD("case", Token::CASE)                                      \
  KEYWORD("catch", Token::CATCH)                                    \
  KEYWORD("class", Token::FUTURE_RESERVED_WORD)                     \
  KEYWORD("const", Token::CONST)                                    \
  KEYWORD("continue", Token::CONTINUE)                              \
  KEYWORD_GROUP('d')                                                \
  KEYWORD("debugger", Token::DEBUGGER)                              \
  KEYWORD("default", Token::DEFAULT)                                \
  KEYWORD("delete", Token::DELETE)                                  \
  KEYWORD("do", Token::DO)                                          \
  KEYWORD_GROUP('e')                                                \
  KEYWORD("else", Token::ELSE)                                      \
  KEYWORD("enum", Token::FUTURE_RESERVED_WORD)                      \
  KEYWORD("export", harmony_modules                                 \
                    ? Token::EXPORT : Token::FUTURE_RESERVED_WORD)  \
  KEYWORD("extends", Token::FUTURE_RESERVED_WORD)                   \
  KEYWORD_GROUP('f')                                                \
  KEYWORD("false", Token::FALSE_LITERAL)                            \
  KEYWORD("finally", Token::FINALLY)                                \
  KEYWORD("for", Token::FOR)                                        \
  KEYWORD("function", Token::FUNCTION)                              \
  KEYWORD_GROUP('i')                                                \
  KEYWORD("if", Token::IF)                                          \
  KEYWORD("implements", Token::FUTURE_STRICT_RESERVED_WORD)         \
  KEYWORD("import", harmony_modules                                 \
                    ? Token::IMPORT : Token::FUTURE_RESERVED_WORD)  \
  KEYWORD("in", Token::IN)                                          \
  KEYWORD("instanceof", Token::INSTANCEOF)                          \
  KEYWORD("interface", Token::FUTURE_STRICT_RESERVED_WORD)          \
  KEYWORD_GROUP('l')                                                \
  KEYWORD("let", harmony_scoping                                    \
                 ? Token::LET : Token::FUTURE_STRICT_RESERVED_WORD) \
  KEYWORD_GROUP('n')                                                \
  KEYWORD("new", Token::NEW)                                        \
  KEYWORD("null", Token::NULL_LITERAL)                              \
  KEYWORD_GROUP('p')                                                \
  KEYWORD("package", Token::FUTURE_STRICT_RESERVED_WORD)            \
  KEYWORD("private", Token::FUTURE_STRICT_RESERVED_WORD)            \
  KEYWORD("protected", Token::FUTURE_STRICT_RESERVED_WORD)          \
  KEYWORD("public", Token::FUTURE_STRICT_RESERVED_WORD)             \
  KEYWORD_GROUP('r')                                                \
  KEYWORD("return", Token::RETURN)                                  \
  KEYWORD_GROUP('s')                                                \
  KEYWORD("static", Token::FUTURE_STRICT_RESERVED_WORD)             \
  KEYWORD("super", Token::FUTURE_RESERVED_WORD)                     \
  KEYWORD("switch", Token::SWITCH)                                  \
  KEYWORD_GROUP('t')                                                \
  KEYWORD("this", Token::THIS)                                      \
  KEYWORD("throw", Token::THROW)                                    \
  KEYWORD("true", Token::TRUE_LITERAL)                              \
  KEYWORD("try", Token::TRY)                                        \
  KEYWORD("typeof", Token::TYPEOF)                                  \
  KEYWORD_GROUP('v')                                                \
  KEYWORD("var", Token::VAR)                                        \
  KEYWORD("void", Token::VOID)                                      \
  KEYWORD_GROUP('w')                                                \
  KEYWORD("while", Token::WHILE)                                    \
  KEYWORD("with", Token::WITH)                                      \
  KEYWORD_GROUP('y')                                                \
  KEYWORD("yield", Token::YIELD)


static Token::Value KeywordOrIdentifierToken(const char* input,
                                             int input_length,
                                             bool harmony_scoping,
                                             bool harmony_modules) {
  ASSERT(input_length >= 1);
  const int kMinLength = 2;
  const int kMaxLength = 10;
  if (input_length < kMinLength || input_length > kMaxLength) {
    return Token::IDENTIFIER;
  }
  switch (input[0]) {
    default:
#define KEYWORD_GROUP_CASE(ch)                                \
      break;                                                  \
    case ch:
#define KEYWORD(keyword, token)                               \
    {                                                         \
      /* 'keyword' is a char array, so sizeof(keyword) is */  \
      /* strlen(keyword) plus 1 for the NUL char. */          \
      const int keyword_length = sizeof(keyword) - 1;         \
      STATIC_ASSERT(keyword_length >= kMinLength);            \
      STATIC_ASSERT(keyword_length <= kMaxLength);            \
      if (input_length == keyword_length &&                   \
          input[1] == keyword[1] &&                           \
          (keyword_length <= 2 || input[2] == keyword[2]) &&  \
          (keyword_length <= 3 || input[3] == keyword[3]) &&  \
          (keyword_length <= 4 || input[4] == keyword[4]) &&  \
          (keyword_length <= 5 || input[5] == keyword[5]) &&  \
          (keyword_length <= 6 || input[6] == keyword[6]) &&  \
          (keyword_length <= 7 || input[7] == keyword[7]) &&  \
          (keyword_length <= 8 || input[8] == keyword[8]) &&  \
          (keyword_length <= 9 || input[9] == keyword[9])) {  \
        return token;                                         \
      }                                                       \
    }
    KEYWORDS(KEYWORD_GROUP_CASE, KEYWORD)
  }
  return Token::IDENTIFIER;
}


Token::Value Scanner::ScanIdentifierOrKeyword() {
  ASSERT(unicode_cache_->IsIdentifierStart(c0_));
  LiteralScope literal(this);
  // Scan identifier start character.
  if (c0_ == '\\') {
    uc32 c = ScanIdentifierUnicodeEscape();
    // Only allow legal identifier start characters.
    if (c < 0 ||
        c == '\\' ||  // No recursive escapes.
        !unicode_cache_->IsIdentifierStart(c)) {
      return Token::ILLEGAL;
    }
    AddLiteralChar(c);
    return ScanIdentifierSuffix(&literal);
  }

  uc32 first_char = c0_;
  Advance();
  AddLiteralChar(first_char);

  // Scan the rest of the identifier characters.
  while (unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ != '\\') {
      uc32 next_char = c0_;
      Advance();
      AddLiteralChar(next_char);
      continue;
    }
    // Fallthrough if no longer able to complete keyword.
    return ScanIdentifierSuffix(&literal);
  }

  literal.Complete();

  if (next_.literal_chars->is_ascii()) {
    Vector<const char> chars = next_.literal_chars->ascii_literal();
    return KeywordOrIdentifierToken(chars.start(),
                                    chars.length(),
                                    harmony_scoping_,
                                    harmony_modules_);
  }

  return Token::IDENTIFIER;
}


Token::Value Scanner::ScanIdentifierSuffix(LiteralScope* literal) {
  // Scan the rest of the identifier characters.
  while (unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      // Only allow legal identifier part characters.
      if (c < 0 ||
          c == '\\' ||
          !unicode_cache_->IsIdentifierPart(c)) {
        return Token::ILLEGAL;
      }
      AddLiteralChar(c);
    } else {
      AddLiteralChar(c0_);
      Advance();
    }
  }
  literal->Complete();

  return Token::IDENTIFIER;
}


bool Scanner::ScanRegExpPattern(bool seen_equal) {
  // Scan: ('/' | '/=') RegularExpressionBody '/' RegularExpressionFlags
  bool in_character_class = false;

  // Previous token is either '/' or '/=', in the second case, the
  // pattern starts at =.
  next_.location.beg_pos = source_pos() - (seen_equal ? 2 : 1);
  next_.location.end_pos = source_pos() - (seen_equal ? 1 : 0);

  // Scan regular expression body: According to ECMA-262, 3rd, 7.8.5,
  // the scanner should pass uninterpreted bodies to the RegExp
  // constructor.
  LiteralScope literal(this);
  if (seen_equal) {
    AddLiteralChar('=');
  }

  while (c0_ != '/' || in_character_class) {
    if (unicode_cache_->IsLineTerminator(c0_) || c0_ < 0) return false;
    if (c0_ == '\\') {  // Escape sequence.
      AddLiteralCharAdvance();
      if (unicode_cache_->IsLineTerminator(c0_) || c0_ < 0) return false;
      AddLiteralCharAdvance();
      // If the escape allows more characters, i.e., \x??, \u????, or \c?,
      // only "safe" characters are allowed (letters, digits, underscore),
      // otherwise the escape isn't valid and the invalid character has
      // its normal meaning. I.e., we can just continue scanning without
      // worrying whether the following characters are part of the escape
      // or not, since any '/', '\\' or '[' is guaranteed to not be part
      // of the escape sequence.

      // TODO(896): At some point, parse RegExps more throughly to capture
      // octal esacpes in strict mode.
    } else {  // Unescaped character.
      if (c0_ == '[') in_character_class = true;
      if (c0_ == ']') in_character_class = false;
      AddLiteralCharAdvance();
    }
  }
  Advance();  // consume '/'

  literal.Complete();

  return true;
}


bool Scanner::ScanLiteralUnicodeEscape() {
  ASSERT(c0_ == '\\');
  uc32 chars_read[6] = {'\\', 'u', 0, 0, 0, 0};
  Advance();
  int i = 1;
  if (c0_ == 'u') {
    i++;
    while (i < 6) {
      Advance();
      if (!IsHexDigit(c0_)) break;
      chars_read[i] = c0_;
      i++;
    }
  }
  if (i < 6) {
    // Incomplete escape. Undo all advances and return false.
    while (i > 0) {
      i--;
      PushBack(chars_read[i]);
    }
    return false;
  }
  // Complete escape. Add all chars to current literal buffer.
  for (int i = 0; i < 6; i++) {
    AddLiteralChar(chars_read[i]);
  }
  return true;
}


bool Scanner::ScanRegExpFlags() {
  // Scan regular expression flags.
  LiteralScope literal(this);
  while (unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ != '\\') {
      AddLiteralCharAdvance();
    } else {
      if (!ScanLiteralUnicodeEscape()) {
        break;
      }
      Advance();
    }
  }
  literal.Complete();

  next_.location.end_pos = source_pos() - 1;
  return true;
}

} }  // namespace v8::internal
