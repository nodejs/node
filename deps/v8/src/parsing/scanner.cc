// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Features shared by parsing and pre-parsing scanners.

#include "src/parsing/scanner.h"

#include <stdint.h>

#include <cmath>

#include "src/ast/ast-value-factory.h"
#include "src/char-predicates-inl.h"
#include "src/conversions-inl.h"
#include "src/list-inl.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {


Handle<String> LiteralBuffer::Internalize(Isolate* isolate) const {
  if (is_one_byte()) {
    return isolate->factory()->InternalizeOneByteString(one_byte_literal());
  }
  return isolate->factory()->InternalizeTwoByteString(two_byte_literal());
}


// Default implementation for streams that do not support bookmarks.
bool Utf16CharacterStream::SetBookmark() { return false; }
void Utf16CharacterStream::ResetToBookmark() { UNREACHABLE(); }


// ----------------------------------------------------------------------------
// Scanner

Scanner::Scanner(UnicodeCache* unicode_cache)
    : unicode_cache_(unicode_cache),
      bookmark_c0_(kNoBookmark),
      octal_pos_(Location::invalid()) {
  bookmark_current_.literal_chars = &bookmark_current_literal_;
  bookmark_current_.raw_literal_chars = &bookmark_current_raw_literal_;
  bookmark_next_.literal_chars = &bookmark_next_literal_;
  bookmark_next_.raw_literal_chars = &bookmark_next_raw_literal_;
}


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


template <bool capture_raw>
uc32 Scanner::ScanHexNumber(int expected_length) {
  DCHECK(expected_length <= 4);  // prevent overflow

  uc32 x = 0;
  for (int i = 0; i < expected_length; i++) {
    int d = HexValue(c0_);
    if (d < 0) {
      return -1;
    }
    x = x * 16 + d;
    Advance<capture_raw>();
  }

  return x;
}


template <bool capture_raw>
uc32 Scanner::ScanUnlimitedLengthHexNumber(int max_value) {
  uc32 x = 0;
  int d = HexValue(c0_);
  if (d < 0) {
    return -1;
  }
  while (d >= 0) {
    x = x * 16 + d;
    if (x > max_value) return -1;
    Advance<capture_raw>();
    d = HexValue(c0_);
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
  if (next_.token == Token::EOS) {
    next_.location.beg_pos = current_.location.beg_pos;
    next_.location.end_pos = current_.location.end_pos;
  }
  current_ = next_;
  if (V8_UNLIKELY(next_next_.token != Token::UNINITIALIZED)) {
    next_ = next_next_;
    next_next_.token = Token::UNINITIALIZED;
    return current_.token;
  }
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


Token::Value Scanner::PeekAhead() {
  if (next_next_.token != Token::UNINITIALIZED) {
    return next_next_.token;
  }
  TokenDesc prev = current_;
  Next();
  Token::Value ret = next_.token;
  next_next_ = next_;
  next_ = current_;
  current_ = prev;
  return ret;
}


// TODO(yangguo): check whether this is actually necessary.
static inline bool IsLittleEndianByteOrderMark(uc32 c) {
  // The Unicode value U+FFFE is guaranteed never to be assigned as a
  // Unicode character; this implies that in a Unicode context the
  // 0xFF, 0xFE byte pattern can only be interpreted as the U+FEFF
  // character expressed in little-endian byte order (since it could
  // not be a U+FFFE character expressed in big-endian byte
  // order). Nevertheless, we check for it to be compatible with
  // Spidermonkey.
  return c == 0xFFFE;
}


bool Scanner::SkipWhiteSpace() {
  int start_position = source_pos();

  while (true) {
    while (true) {
      // The unicode cache accepts unsigned inputs.
      if (c0_ < 0) break;
      // Advance as long as character is a WhiteSpace or LineTerminator.
      // Remember if the latter is the case.
      if (unicode_cache_->IsLineTerminator(c0_)) {
        has_line_terminator_before_next_ = true;
      } else if (!unicode_cache_->IsWhiteSpace(c0_) &&
                 !IsLittleEndianByteOrderMark(c0_)) {
        break;
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


Token::Value Scanner::SkipSourceURLComment() {
  TryToParseSourceURLComment();
  while (c0_ >= 0 && !unicode_cache_->IsLineTerminator(c0_)) {
    Advance();
  }

  return Token::WHITESPACE;
}


void Scanner::TryToParseSourceURLComment() {
  // Magic comments are of the form: //[#@]\s<name>=\s*<value>\s*.* and this
  // function will just return if it cannot parse a magic comment.
  if (c0_ < 0 || !unicode_cache_->IsWhiteSpace(c0_)) return;
  Advance();
  LiteralBuffer name;
  while (c0_ >= 0 && !unicode_cache_->IsWhiteSpaceOrLineTerminator(c0_) &&
         c0_ != '=') {
    name.AddChar(c0_);
    Advance();
  }
  if (!name.is_one_byte()) return;
  Vector<const uint8_t> name_literal = name.one_byte_literal();
  LiteralBuffer* value;
  if (name_literal == STATIC_CHAR_VECTOR("sourceURL")) {
    value = &source_url_;
  } else if (name_literal == STATIC_CHAR_VECTOR("sourceMappingURL")) {
    value = &source_mapping_url_;
  } else {
    return;
  }
  if (c0_ != '=')
    return;
  Advance();
  value->Reset();
  while (c0_ >= 0 && unicode_cache_->IsWhiteSpace(c0_)) {
    Advance();
  }
  while (c0_ >= 0 && !unicode_cache_->IsLineTerminator(c0_)) {
    // Disallowed characters.
    if (c0_ == '"' || c0_ == '\'') {
      value->Reset();
      return;
    }
    if (unicode_cache_->IsWhiteSpace(c0_)) {
      break;
    }
    value->AddChar(c0_);
    Advance();
  }
  // Allow whitespace at the end.
  while (c0_ >= 0 && !unicode_cache_->IsLineTerminator(c0_)) {
    if (!unicode_cache_->IsWhiteSpace(c0_)) {
      value->Reset();
      break;
    }
    Advance();
  }
}


Token::Value Scanner::SkipMultiLineComment() {
  DCHECK(c0_ == '*');
  Advance();

  while (c0_ >= 0) {
    uc32 ch = c0_;
    Advance();
    if (c0_ >= 0 && unicode_cache_->IsLineTerminator(ch)) {
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
  DCHECK(c0_ == '!');
  Advance();
  if (c0_ == '-') {
    Advance();
    if (c0_ == '-') return SkipSingleLineComment();
    PushBack('-');  // undo Advance()
  }
  PushBack('!');  // undo Advance()
  DCHECK(c0_ == '!');
  return Token::LT;
}


void Scanner::Scan() {
  next_.literal_chars = NULL;
  next_.raw_literal_chars = NULL;
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
        // = == === =>
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::EQ_STRICT, Token::EQ);
        } else if (c0_ == '>') {
          token = Select(Token::ARROW);
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
          Advance();
          if (c0_ == '#' || c0_ == '@') {
            Advance();
            token = SkipSourceURLComment();
          } else {
            PushBack(c0_);
            token = SkipSingleLineComment();
          }
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
          if (c0_ == '.') {
            Advance();
            if (c0_ == '.') {
              Advance();
              token = Token::ELLIPSIS;
            } else {
              PushBack('.');
            }
          }
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

      case '`':
        token = ScanTemplateStart();
        break;

      default:
        if (c0_ < 0) {
          token = Token::EOS;
        } else if (unicode_cache_->IsIdentifierStart(c0_)) {
          token = ScanIdentifierOrKeyword();
        } else if (IsDecimalDigit(c0_)) {
          token = ScanNumber(false);
        } else if (SkipWhiteSpace()) {
          token = Token::WHITESPACE;
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
  DCHECK_EQ(next_.location.end_pos, current_pos);
  // Positions inside the lookahead token aren't supported.
  DCHECK(pos >= current_pos);
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


template <bool capture_raw, bool in_template_literal>
bool Scanner::ScanEscape() {
  uc32 c = c0_;
  Advance<capture_raw>();

  // Skip escaped newlines.
  if (!in_template_literal && c0_ >= 0 && unicode_cache_->IsLineTerminator(c)) {
    // Allow CR+LF newlines in multiline string literals.
    if (IsCarriageReturn(c) && IsLineFeed(c0_)) Advance<capture_raw>();
    // Allow LF+CR newlines in multiline string literals.
    if (IsLineFeed(c) && IsCarriageReturn(c0_)) Advance<capture_raw>();
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
      c = ScanUnicodeEscape<capture_raw>();
      if (c < 0) return false;
      break;
    }
    case 'v':
      c = '\v';
      break;
    case 'x': {
      c = ScanHexNumber<capture_raw>(2);
      if (c < 0) return false;
      break;
    }
    case '0':  // Fall through.
    case '1':  // fall through
    case '2':  // fall through
    case '3':  // fall through
    case '4':  // fall through
    case '5':  // fall through
    case '6':  // fall through
    case '7':
      c = ScanOctalEscape<capture_raw>(c, 2);
      break;
  }

  // According to ECMA-262, section 7.8.4, characters not covered by the
  // above cases should be illegal, but they are commonly handled as
  // non-escaped characters by JS VMs.
  AddLiteralChar(c);
  return true;
}


// Octal escapes of the forms '\0xx' and '\xxx' are not a part of
// ECMA-262. Other JS VMs support them.
template <bool capture_raw>
uc32 Scanner::ScanOctalEscape(uc32 c, int length) {
  uc32 x = c - '0';
  int i = 0;
  for (; i < length; i++) {
    int d = c0_ - '0';
    if (d < 0 || d > 7) break;
    int nx = x * 8 + d;
    if (nx >= 256) break;
    x = nx;
    Advance<capture_raw>();
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


const int kMaxAscii = 127;


Token::Value Scanner::ScanString() {
  uc32 quote = c0_;
  Advance<false, false>();  // consume quote

  LiteralScope literal(this);
  while (true) {
    if (c0_ > kMaxAscii) {
      HandleLeadSurrogate();
      break;
    }
    if (c0_ < 0 || c0_ == '\n' || c0_ == '\r') return Token::ILLEGAL;
    if (c0_ == quote) {
      literal.Complete();
      Advance<false, false>();
      return Token::STRING;
    }
    uc32 c = c0_;
    if (c == '\\') break;
    Advance<false, false>();
    AddLiteralChar(c);
  }

  while (c0_ != quote && c0_ >= 0
         && !unicode_cache_->IsLineTerminator(c0_)) {
    uc32 c = c0_;
    Advance();
    if (c == '\\') {
      if (c0_ < 0 || !ScanEscape<false, false>()) return Token::ILLEGAL;
    } else {
      AddLiteralChar(c);
    }
  }
  if (c0_ != quote) return Token::ILLEGAL;
  literal.Complete();

  Advance();  // consume quote
  return Token::STRING;
}


Token::Value Scanner::ScanTemplateSpan() {
  // When scanning a TemplateSpan, we are looking for the following construct:
  // TEMPLATE_SPAN ::
  //     ` LiteralChars* ${
  //   | } LiteralChars* ${
  //
  // TEMPLATE_TAIL ::
  //     ` LiteralChars* `
  //   | } LiteralChar* `
  //
  // A TEMPLATE_SPAN should always be followed by an Expression, while a
  // TEMPLATE_TAIL terminates a TemplateLiteral and does not need to be
  // followed by an Expression.

  Token::Value result = Token::TEMPLATE_SPAN;
  LiteralScope literal(this);
  StartRawLiteral();
  const bool capture_raw = true;
  const bool in_template_literal = true;

  while (true) {
    uc32 c = c0_;
    Advance<capture_raw>();
    if (c == '`') {
      result = Token::TEMPLATE_TAIL;
      ReduceRawLiteralLength(1);
      break;
    } else if (c == '$' && c0_ == '{') {
      Advance<capture_raw>();  // Consume '{'
      ReduceRawLiteralLength(2);
      break;
    } else if (c == '\\') {
      if (c0_ > 0 && unicode_cache_->IsLineTerminator(c0_)) {
        // The TV of LineContinuation :: \ LineTerminatorSequence is the empty
        // code unit sequence.
        uc32 lastChar = c0_;
        Advance<capture_raw>();
        if (lastChar == '\r') {
          ReduceRawLiteralLength(1);  // Remove \r
          if (c0_ == '\n') {
            Advance<capture_raw>();  // Adds \n
          } else {
            AddRawLiteralChar('\n');
          }
        }
      } else if (!ScanEscape<capture_raw, in_template_literal>()) {
        return Token::ILLEGAL;
      }
    } else if (c < 0) {
      // Unterminated template literal
      PushBack(c);
      break;
    } else {
      // The TRV of LineTerminatorSequence :: <CR> is the CV 0x000A.
      // The TRV of LineTerminatorSequence :: <CR><LF> is the sequence
      // consisting of the CV 0x000A.
      if (c == '\r') {
        ReduceRawLiteralLength(1);  // Remove \r
        if (c0_ == '\n') {
          Advance<capture_raw>();  // Adds \n
        } else {
          AddRawLiteralChar('\n');
        }
        c = '\n';
      }
      AddLiteralChar(c);
    }
  }
  literal.Complete();
  next_.location.end_pos = source_pos();
  next_.token = result;
  return result;
}


Token::Value Scanner::ScanTemplateStart() {
  DCHECK(c0_ == '`');
  next_.location.beg_pos = source_pos();
  Advance();  // Consume `
  return ScanTemplateSpan();
}


Token::Value Scanner::ScanTemplateContinuation() {
  DCHECK_EQ(next_.token, Token::RBRACE);
  next_.location.beg_pos = source_pos() - 1;  // We already consumed }
  return ScanTemplateSpan();
}


void Scanner::ScanDecimalDigits() {
  while (IsDecimalDigit(c0_))
    AddLiteralCharAdvance();
}


Token::Value Scanner::ScanNumber(bool seen_period) {
  DCHECK(IsDecimalDigit(c0_));  // the first digit of the number or the fraction

  enum { DECIMAL, HEX, OCTAL, IMPLICIT_OCTAL, BINARY } kind = DECIMAL;

  LiteralScope literal(this);
  bool at_start = !seen_period;
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
      } else if (c0_ == 'o' || c0_ == 'O') {
        kind = OCTAL;
        AddLiteralCharAdvance();
        if (!IsOctalDigit(c0_)) {
          // we must have at least one octal digit after 'o'/'O'
          return Token::ILLEGAL;
        }
        while (IsOctalDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if (c0_ == 'b' || c0_ == 'B') {
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
            at_start = false;
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
      if (at_start) {
        uint64_t value = 0;
        while (IsDecimalDigit(c0_)) {
          value = 10 * value + (c0_ - '0');

          uc32 first_char = c0_;
          Advance<false, false>();
          AddLiteralChar(first_char);
        }

        if (next_.literal_chars->one_byte_literal().length() <= 10 &&
            value <= Smi::kMaxValue && c0_ != '.' && c0_ != 'e' && c0_ != 'E') {
          next_.smi_value_ = static_cast<int>(value);
          literal.Complete();
          HandleLeadSurrogate();

          return Token::SMI;
        }
        HandleLeadSurrogate();
      }

      ScanDecimalDigits();  // optional
      if (c0_ == '.') {
        AddLiteralCharAdvance();
        ScanDecimalDigits();  // optional
      }
    }
  }

  // scan exponent, if any
  if (c0_ == 'e' || c0_ == 'E') {
    DCHECK(kind != HEX);  // 'e'/'E' must be scanned as part of the hex number
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
  if (IsDecimalDigit(c0_) ||
      (c0_ >= 0 && unicode_cache_->IsIdentifierStart(c0_)))
    return Token::ILLEGAL;

  literal.Complete();

  return Token::NUMBER;
}


uc32 Scanner::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return -1;
  Advance();
  return ScanUnicodeEscape<false>();
}


template <bool capture_raw>
uc32 Scanner::ScanUnicodeEscape() {
  // Accept both \uxxxx and \u{xxxxxx}. In the latter case, the number of
  // hex digits between { } is arbitrary. \ and u have already been read.
  if (c0_ == '{') {
    Advance<capture_raw>();
    uc32 cp = ScanUnlimitedLengthHexNumber<capture_raw>(0x10ffff);
    if (cp < 0) {
      return -1;
    }
    if (c0_ != '}') {
      return -1;
    }
    Advance<capture_raw>();
    return cp;
  }
  return ScanHexNumber<capture_raw>(4);
}


// ----------------------------------------------------------------------------
// Keyword Matcher

#define KEYWORDS(KEYWORD_GROUP, KEYWORD)                    \
  KEYWORD_GROUP('b')                                        \
  KEYWORD("break", Token::BREAK)                            \
  KEYWORD_GROUP('c')                                        \
  KEYWORD("case", Token::CASE)                              \
  KEYWORD("catch", Token::CATCH)                            \
  KEYWORD("class", Token::CLASS)                            \
  KEYWORD("const", Token::CONST)                            \
  KEYWORD("continue", Token::CONTINUE)                      \
  KEYWORD_GROUP('d')                                        \
  KEYWORD("debugger", Token::DEBUGGER)                      \
  KEYWORD("default", Token::DEFAULT)                        \
  KEYWORD("delete", Token::DELETE)                          \
  KEYWORD("do", Token::DO)                                  \
  KEYWORD_GROUP('e')                                        \
  KEYWORD("else", Token::ELSE)                              \
  KEYWORD("enum", Token::FUTURE_RESERVED_WORD)              \
  KEYWORD("export", Token::EXPORT)                          \
  KEYWORD("extends", Token::EXTENDS)                        \
  KEYWORD_GROUP('f')                                        \
  KEYWORD("false", Token::FALSE_LITERAL)                    \
  KEYWORD("finally", Token::FINALLY)                        \
  KEYWORD("for", Token::FOR)                                \
  KEYWORD("function", Token::FUNCTION)                      \
  KEYWORD_GROUP('i')                                        \
  KEYWORD("if", Token::IF)                                  \
  KEYWORD("implements", Token::FUTURE_STRICT_RESERVED_WORD) \
  KEYWORD("import", Token::IMPORT)                          \
  KEYWORD("in", Token::IN)                                  \
  KEYWORD("instanceof", Token::INSTANCEOF)                  \
  KEYWORD("interface", Token::FUTURE_STRICT_RESERVED_WORD)  \
  KEYWORD_GROUP('l')                                        \
  KEYWORD("let", Token::LET)                                \
  KEYWORD_GROUP('n')                                        \
  KEYWORD("new", Token::NEW)                                \
  KEYWORD("null", Token::NULL_LITERAL)                      \
  KEYWORD_GROUP('p')                                        \
  KEYWORD("package", Token::FUTURE_STRICT_RESERVED_WORD)    \
  KEYWORD("private", Token::FUTURE_STRICT_RESERVED_WORD)    \
  KEYWORD("protected", Token::FUTURE_STRICT_RESERVED_WORD)  \
  KEYWORD("public", Token::FUTURE_STRICT_RESERVED_WORD)     \
  KEYWORD_GROUP('r')                                        \
  KEYWORD("return", Token::RETURN)                          \
  KEYWORD_GROUP('s')                                        \
  KEYWORD("static", Token::STATIC)                          \
  KEYWORD("super", Token::SUPER)                            \
  KEYWORD("switch", Token::SWITCH)                          \
  KEYWORD_GROUP('t')                                        \
  KEYWORD("this", Token::THIS)                              \
  KEYWORD("throw", Token::THROW)                            \
  KEYWORD("true", Token::TRUE_LITERAL)                      \
  KEYWORD("try", Token::TRY)                                \
  KEYWORD("typeof", Token::TYPEOF)                          \
  KEYWORD_GROUP('v')                                        \
  KEYWORD("var", Token::VAR)                                \
  KEYWORD("void", Token::VOID)                              \
  KEYWORD_GROUP('w')                                        \
  KEYWORD("while", Token::WHILE)                            \
  KEYWORD("with", Token::WITH)                              \
  KEYWORD_GROUP('y')                                        \
  KEYWORD("yield", Token::YIELD)


static Token::Value KeywordOrIdentifierToken(const uint8_t* input,
                                             int input_length, bool escaped) {
  DCHECK(input_length >= 1);
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
#define KEYWORD(keyword, token)                                     \
  {                                                                 \
    /* 'keyword' is a char array, so sizeof(keyword) is */          \
    /* strlen(keyword) plus 1 for the NUL char. */                  \
    const int keyword_length = sizeof(keyword) - 1;                 \
    STATIC_ASSERT(keyword_length >= kMinLength);                    \
    STATIC_ASSERT(keyword_length <= kMaxLength);                    \
    if (input_length == keyword_length && input[1] == keyword[1] && \
        (keyword_length <= 2 || input[2] == keyword[2]) &&          \
        (keyword_length <= 3 || input[3] == keyword[3]) &&          \
        (keyword_length <= 4 || input[4] == keyword[4]) &&          \
        (keyword_length <= 5 || input[5] == keyword[5]) &&          \
        (keyword_length <= 6 || input[6] == keyword[6]) &&          \
        (keyword_length <= 7 || input[7] == keyword[7]) &&          \
        (keyword_length <= 8 || input[8] == keyword[8]) &&          \
        (keyword_length <= 9 || input[9] == keyword[9])) {          \
      if (escaped) {                                                \
        return token == Token::FUTURE_STRICT_RESERVED_WORD          \
                   ? Token::ESCAPED_STRICT_RESERVED_WORD            \
                   : Token::ESCAPED_KEYWORD;                        \
      }                                                             \
      return token;                                                 \
    }                                                               \
  }
    KEYWORDS(KEYWORD_GROUP_CASE, KEYWORD)
  }
  return Token::IDENTIFIER;
}


bool Scanner::IdentifierIsFutureStrictReserved(
    const AstRawString* string) const {
  // Keywords are always 1-byte strings.
  if (!string->is_one_byte()) return false;
  if (string->IsOneByteEqualTo("let") || string->IsOneByteEqualTo("static") ||
      string->IsOneByteEqualTo("yield")) {
    return true;
  }
  return Token::FUTURE_STRICT_RESERVED_WORD ==
         KeywordOrIdentifierToken(string->raw_data(), string->length(), false);
}


Token::Value Scanner::ScanIdentifierOrKeyword() {
  DCHECK(unicode_cache_->IsIdentifierStart(c0_));
  LiteralScope literal(this);
  if (IsInRange(c0_, 'a', 'z')) {
    do {
      uc32 first_char = c0_;
      Advance<false, false>();
      AddLiteralChar(first_char);
    } while (IsInRange(c0_, 'a', 'z'));

    if (IsDecimalDigit(c0_) || IsInRange(c0_, 'A', 'Z') || c0_ == '_' ||
        c0_ == '$') {
      // Identifier starting with lowercase.
      uc32 first_char = c0_;
      Advance<false, false>();
      AddLiteralChar(first_char);
      while (IsAsciiIdentifier(c0_)) {
        uc32 first_char = c0_;
        Advance<false, false>();
        AddLiteralChar(first_char);
      }
      if (c0_ <= kMaxAscii && c0_ != '\\') {
        literal.Complete();
        return Token::IDENTIFIER;
      }
    } else if (c0_ <= kMaxAscii && c0_ != '\\') {
      // Only a-z+: could be a keyword or identifier.
      literal.Complete();
      Vector<const uint8_t> chars = next_.literal_chars->one_byte_literal();
      return KeywordOrIdentifierToken(chars.start(), chars.length(), false);
    }

    HandleLeadSurrogate();
  } else if (IsInRange(c0_, 'A', 'Z') || c0_ == '_' || c0_ == '$') {
    do {
      uc32 first_char = c0_;
      Advance<false, false>();
      AddLiteralChar(first_char);
    } while (IsAsciiIdentifier(c0_));

    if (c0_ <= kMaxAscii && c0_ != '\\') {
      literal.Complete();
      return Token::IDENTIFIER;
    }

    HandleLeadSurrogate();
  } else if (c0_ == '\\') {
    // Scan identifier start character.
    uc32 c = ScanIdentifierUnicodeEscape();
    // Only allow legal identifier start characters.
    if (c < 0 ||
        c == '\\' ||  // No recursive escapes.
        !unicode_cache_->IsIdentifierStart(c)) {
      return Token::ILLEGAL;
    }
    AddLiteralChar(c);
    return ScanIdentifierSuffix(&literal, true);
  } else {
    uc32 first_char = c0_;
    Advance();
    AddLiteralChar(first_char);
  }

  // Scan the rest of the identifier characters.
  while (c0_ >= 0 && unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ != '\\') {
      uc32 next_char = c0_;
      Advance();
      AddLiteralChar(next_char);
      continue;
    }
    // Fallthrough if no longer able to complete keyword.
    return ScanIdentifierSuffix(&literal, false);
  }

  literal.Complete();

  if (next_.literal_chars->is_one_byte()) {
    Vector<const uint8_t> chars = next_.literal_chars->one_byte_literal();
    return KeywordOrIdentifierToken(chars.start(), chars.length(), false);
  }
  return Token::IDENTIFIER;
}


Token::Value Scanner::ScanIdentifierSuffix(LiteralScope* literal,
                                           bool escaped) {
  // Scan the rest of the identifier characters.
  while (c0_ >= 0 && unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      escaped = true;
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

  if (escaped && next_.literal_chars->is_one_byte()) {
    Vector<const uint8_t> chars = next_.literal_chars->one_byte_literal();
    return KeywordOrIdentifierToken(chars.start(), chars.length(), true);
  }
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
    if (c0_ < 0 || unicode_cache_->IsLineTerminator(c0_)) return false;
    if (c0_ == '\\') {  // Escape sequence.
      AddLiteralCharAdvance();
      if (c0_ < 0 || unicode_cache_->IsLineTerminator(c0_)) return false;
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


Maybe<RegExp::Flags> Scanner::ScanRegExpFlags() {
  // Scan regular expression flags.
  LiteralScope literal(this);
  int flags = 0;
  while (c0_ >= 0 && unicode_cache_->IsIdentifierPart(c0_)) {
    RegExp::Flags flag = RegExp::kNone;
    switch (c0_) {
      case 'g':
        flag = RegExp::kGlobal;
        break;
      case 'i':
        flag = RegExp::kIgnoreCase;
        break;
      case 'm':
        flag = RegExp::kMultiline;
        break;
      case 'u':
        if (!FLAG_harmony_unicode_regexps) return Nothing<RegExp::Flags>();
        flag = RegExp::kUnicode;
        break;
      case 'y':
        if (!FLAG_harmony_regexps) return Nothing<RegExp::Flags>();
        flag = RegExp::kSticky;
        break;
      default:
        return Nothing<RegExp::Flags>();
    }
    if (flags & flag) return Nothing<RegExp::Flags>();
    AddLiteralCharAdvance();
    flags |= flag;
  }
  literal.Complete();

  next_.location.end_pos = source_pos();
  return Just(RegExp::Flags(flags));
}


const AstRawString* Scanner::CurrentSymbol(AstValueFactory* ast_value_factory) {
  if (is_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(literal_two_byte_string());
}


const AstRawString* Scanner::NextSymbol(AstValueFactory* ast_value_factory) {
  if (is_next_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(next_literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(next_literal_two_byte_string());
}


const AstRawString* Scanner::CurrentRawSymbol(
    AstValueFactory* ast_value_factory) {
  if (is_raw_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(raw_literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(raw_literal_two_byte_string());
}


double Scanner::DoubleValue() {
  DCHECK(is_literal_one_byte());
  return StringToDouble(
      unicode_cache_,
      literal_one_byte_string(),
      ALLOW_HEX | ALLOW_OCTAL | ALLOW_IMPLICIT_OCTAL | ALLOW_BINARY);
}


bool Scanner::ContainsDot() {
  DCHECK(is_literal_one_byte());
  Vector<const uint8_t> str = literal_one_byte_string();
  return std::find(str.begin(), str.end(), '.') != str.end();
}


int Scanner::FindSymbol(DuplicateFinder* finder, int value) {
  if (is_literal_one_byte()) {
    return finder->AddOneByteSymbol(literal_one_byte_string(), value);
  }
  return finder->AddTwoByteSymbol(literal_two_byte_string(), value);
}


bool Scanner::SetBookmark() {
  if (c0_ != kNoBookmark && bookmark_c0_ == kNoBookmark &&
      next_next_.token == Token::UNINITIALIZED && source_->SetBookmark()) {
    bookmark_c0_ = c0_;
    CopyTokenDesc(&bookmark_current_, &current_);
    CopyTokenDesc(&bookmark_next_, &next_);
    return true;
  }
  return false;
}


void Scanner::ResetToBookmark() {
  DCHECK(BookmarkHasBeenSet());  // Caller hasn't called SetBookmark.

  source_->ResetToBookmark();
  c0_ = bookmark_c0_;
  StartLiteral();
  StartRawLiteral();
  CopyTokenDesc(&next_, &bookmark_current_);
  current_ = next_;
  StartLiteral();
  StartRawLiteral();
  CopyTokenDesc(&next_, &bookmark_next_);

  bookmark_c0_ = kBookmarkWasApplied;
}


bool Scanner::BookmarkHasBeenSet() { return bookmark_c0_ >= 0; }


bool Scanner::BookmarkHasBeenReset() {
  return bookmark_c0_ == kBookmarkWasApplied;
}


void Scanner::DropBookmark() { bookmark_c0_ = kNoBookmark; }


void Scanner::CopyTokenDesc(TokenDesc* to, TokenDesc* from) {
  DCHECK_NOT_NULL(to);
  DCHECK_NOT_NULL(from);
  to->token = from->token;
  to->location = from->location;
  to->literal_chars->CopyFrom(from->literal_chars);
  to->raw_literal_chars->CopyFrom(from->raw_literal_chars);
}


int DuplicateFinder::AddOneByteSymbol(Vector<const uint8_t> key, int value) {
  return AddSymbol(key, true, value);
}


int DuplicateFinder::AddTwoByteSymbol(Vector<const uint16_t> key, int value) {
  return AddSymbol(Vector<const uint8_t>::cast(key), false, value);
}


int DuplicateFinder::AddSymbol(Vector<const uint8_t> key,
                               bool is_one_byte,
                               int value) {
  uint32_t hash = Hash(key, is_one_byte);
  byte* encoding = BackupKey(key, is_one_byte);
  HashMap::Entry* entry = map_.LookupOrInsert(encoding, hash);
  int old_value = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  entry->value =
    reinterpret_cast<void*>(static_cast<intptr_t>(value | old_value));
  return old_value;
}


int DuplicateFinder::AddNumber(Vector<const uint8_t> key, int value) {
  DCHECK(key.length() > 0);
  // Quick check for already being in canonical form.
  if (IsNumberCanonical(key)) {
    return AddOneByteSymbol(key, value);
  }

  int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_IMPLICIT_OCTAL | ALLOW_BINARY;
  double double_value = StringToDouble(
      unicode_constants_, key, flags, 0.0);
  int length;
  const char* string;
  if (!std::isfinite(double_value)) {
    string = "Infinity";
    length = 8;  // strlen("Infinity");
  } else {
    string = DoubleToCString(double_value,
                             Vector<char>(number_buffer_, kBufferSize));
    length = StrLength(string);
  }
  return AddSymbol(Vector<const byte>(reinterpret_cast<const byte*>(string),
                                      length), true, value);
}


bool DuplicateFinder::IsNumberCanonical(Vector<const uint8_t> number) {
  // Test for a safe approximation of number literals that are already
  // in canonical form: max 15 digits, no leading zeroes, except an
  // integer part that is a single zero, and no trailing zeros below
  // the decimal point.
  int pos = 0;
  int length = number.length();
  if (number.length() > 15) return false;
  if (number[pos] == '0') {
    pos++;
  } else {
    while (pos < length &&
           static_cast<unsigned>(number[pos] - '0') <= ('9' - '0')) pos++;
  }
  if (length == pos) return true;
  if (number[pos] != '.') return false;
  pos++;
  bool invalid_last_digit = true;
  while (pos < length) {
    uint8_t digit = number[pos] - '0';
    if (digit > '9' - '0') return false;
    invalid_last_digit = (digit == 0);
    pos++;
  }
  return !invalid_last_digit;
}


uint32_t DuplicateFinder::Hash(Vector<const uint8_t> key, bool is_one_byte) {
  // Primitive hash function, almost identical to the one used
  // for strings (except that it's seeded by the length and representation).
  int length = key.length();
  uint32_t hash = (length << 1) | (is_one_byte ? 1 : 0);
  for (int i = 0; i < length; i++) {
    uint32_t c = key[i];
    hash = (hash + c) * 1025;
    hash ^= (hash >> 6);
  }
  return hash;
}


bool DuplicateFinder::Match(void* first, void* second) {
  // Decode lengths.
  // Length + representation is encoded as base 128, most significant heptet
  // first, with a 8th bit being non-zero while there are more heptets.
  // The value encodes the number of bytes following, and whether the original
  // was Latin1.
  byte* s1 = reinterpret_cast<byte*>(first);
  byte* s2 = reinterpret_cast<byte*>(second);
  uint32_t length_one_byte_field = 0;
  byte c1;
  do {
    c1 = *s1;
    if (c1 != *s2) return false;
    length_one_byte_field = (length_one_byte_field << 7) | (c1 & 0x7f);
    s1++;
    s2++;
  } while ((c1 & 0x80) != 0);
  int length = static_cast<int>(length_one_byte_field >> 1);
  return memcmp(s1, s2, length) == 0;
}


byte* DuplicateFinder::BackupKey(Vector<const uint8_t> bytes,
                                 bool is_one_byte) {
  uint32_t one_byte_length = (bytes.length() << 1) | (is_one_byte ? 1 : 0);
  backing_store_.StartSequence();
  // Emit one_byte_length as base-128 encoded number, with the 7th bit set
  // on the byte of every heptet except the last, least significant, one.
  if (one_byte_length >= (1 << 7)) {
    if (one_byte_length >= (1 << 14)) {
      if (one_byte_length >= (1 << 21)) {
        if (one_byte_length >= (1 << 28)) {
          backing_store_.Add(
              static_cast<uint8_t>((one_byte_length >> 28) | 0x80));
        }
        backing_store_.Add(
            static_cast<uint8_t>((one_byte_length >> 21) | 0x80u));
      }
      backing_store_.Add(
          static_cast<uint8_t>((one_byte_length >> 14) | 0x80u));
    }
    backing_store_.Add(static_cast<uint8_t>((one_byte_length >> 7) | 0x80u));
  }
  backing_store_.Add(static_cast<uint8_t>(one_byte_length & 0x7f));

  backing_store_.AddBlock(bytes);
  return backing_store_.EndSequence().start();
}

}  // namespace internal
}  // namespace v8
