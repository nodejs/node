// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_PRESCANNER_H_
#define V8_PRESCANNER_H_

#include "token.h"
#include "char-predicates-inl.h"
#include "utils.h"
#include "scanner-base.h"

namespace v8 {
namespace preparser {

namespace i = v8::internal;

typedef int uc32;

int HexValue(uc32 c) {
  int res = c | 0x20;  // Uppercase letters.
  int is_digit = (c & 0x10) >> 4;  // 0 if non-digit, 1 if digit.
  // What to add to digits to make them consecutive with 'a'-'f' letters.
  int kDelta = 'a' - '9' - 1;
  // What to subtract to digits and letters to get them back to the range 0..15.
  int kStart = '0' + kDelta;
  res -= kStart;
  res += kDelta * is_digit;
  return res;
}


class PreScannerStackGuard {
 public:
  explicit PreScannerStackGuard(int max_size)
      : limit_(StackPoint().at() - max_size) { }
  bool has_overflowed() {
    return StackPoint().at() < limit_;
  }
 private:
  class StackPoint {
   public:
    char* at() { return reinterpret_cast<char*>(this); }
  };
  char* limit_;
};


// Scanner for preparsing.
// InputStream is a source of UC16 characters with limited push-back.
// LiteralsBuffer is a collector of (UTF-8) characters used to capture literals.
template <typename InputStream, typename LiteralsBuffer>
class Scanner {
 public:
  enum LiteralType {
    kLiteralNumber,
    kLiteralIdentifier,
    kLiteralString,
    kLiteralRegExp,
    kLiteralRegExpFlags
  };

  class LiteralScope {
   public:
    explicit LiteralScope(Scanner* self, LiteralType type);
    ~LiteralScope();
    void Complete();

   private:
    Scanner* scanner_;
    bool complete_;
  };

  Scanner();

  void Initialize(InputStream* stream);

  // Returns the next token.
  i::Token::Value Next();

  // Returns the current token again.
  i::Token::Value current_token() { return current_.token; }

  // One token look-ahead (past the token returned by Next()).
  i::Token::Value peek() const { return next_.token; }

  // Returns true if there was a line terminator before the peek'ed token.
  bool has_line_terminator_before_next() const {
    return has_line_terminator_before_next_;
  }

  struct Location {
    Location(int b, int e) : beg_pos(b), end_pos(e) { }
    Location() : beg_pos(0), end_pos(0) { }
    int beg_pos;
    int end_pos;
  };

  // Returns the location information for the current token
  // (the token returned by Next()).
  Location location() const { return current_.location; }
  // Returns the location information for the look-ahead token
  // (the token returned by peek()).
  Location peek_location() const { return next_.location; }

  // Returns the literal string, if any, for the current token (the
  // token returned by Next()). The string is 0-terminated and in
  // UTF-8 format; they may contain 0-characters. Literal strings are
  // collected for identifiers, strings, and numbers.
  // These functions only give the correct result if the literal
  // was scanned between calls to StartLiteral() and TerminateLiteral().
  const char* literal_string() const {
    return current_.literal_chars;
  }

  int literal_length() const {
    // Excluding terminal '\x00' added by TerminateLiteral().
    return current_.literal_length - 1;
  }

  i::Vector<const char> literal() const {
    return i::Vector<const char>(literal_string(), literal_length());
  }

  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  const char* next_literal_string() const {
    return next_.literal_chars;
  }


  // Returns the length of the next token (that would be returned if
  // Next() were called).
  int next_literal_length() const {
    // Excluding terminal '\x00' added by TerminateLiteral().
    return next_.literal_length - 1;
  }

  i::Vector<const char> next_literal() const {
    return i::Vector<const char>(next_literal_string(), next_literal_length());
  }

  // Scans the input as a regular expression pattern, previous
  // character(s) must be /(=). Returns true if a pattern is scanned.
  bool ScanRegExpPattern(bool seen_equal);
  // Returns true if regexp flags are scanned (always since flags can
  // be empty).
  bool ScanRegExpFlags();

  // Seek forward to the given position.  This operation does not
  // work in general, for instance when there are pushed back
  // characters, but works for seeking forward until simple delimiter
  // tokens, which is what it is used for.
  void SeekForward(int pos);

  bool stack_overflow() { return stack_overflow_; }

  static const int kCharacterLookaheadBufferSize = 1;
  static const int kNoEndPosition = 1;

 private:
  // The current and look-ahead token.
  struct TokenDesc {
    i::Token::Value token;
    Location location;
    const char* literal_chars;
    int literal_length;
  };

  // Default stack limit is 128K pointers.
  static const int kMaxStackSize = 128 * 1024 * sizeof(void*);  // NOLINT.

  void Init(unibrow::CharacterStream* stream);

  // Literal buffer support
  inline void StartLiteral(LiteralType type);
  inline void AddLiteralChar(uc32 ch);
  inline void AddLiteralCharAdvance();
  inline void TerminateLiteral();
  // Stops scanning of a literal, e.g., due to an encountered error.
  inline void DropLiteral();

  // Low-level scanning support.
  void Advance() { c0_ = source_->Advance(); }
  void PushBack(uc32 ch) {
    source_->PushBack(ch);
    c0_ = ch;
  }

  bool SkipWhiteSpace();

  i::Token::Value SkipSingleLineComment();
  i::Token::Value SkipMultiLineComment();

  inline i::Token::Value Select(i::Token::Value tok);
  inline i::Token::Value Select(uc32 next,
                                i::Token::Value then,
                                i::Token::Value else_);

  // Scans a single JavaScript token.
  void Scan();

  void ScanDecimalDigits();
  i::Token::Value ScanNumber(bool seen_period);
  i::Token::Value ScanIdentifier();
  uc32 ScanHexEscape(uc32 c, int length);
  uc32 ScanOctalEscape(uc32 c, int length);
  void ScanEscape();
  i::Token::Value ScanString();

  // Scans a possible HTML comment -- begins with '<!'.
  i::Token::Value ScanHtmlComment();

  // Return the current source position.
  int source_pos() {
    return source_->pos() - kCharacterLookaheadBufferSize;
  }

  // Decodes a unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadRune.
  uc32 ScanIdentifierUnicodeEscape();

  PreScannerStackGuard stack_guard_;

  TokenDesc current_;  // desc for current token (as returned by Next())
  TokenDesc next_;     // desc for next token (one token look-ahead)
  bool has_line_terminator_before_next_;

  // Source.
  InputStream* source_;

  // Buffer to hold literal values (identifiers, strings, numerals, regexps and
  // regexp flags) using '\x00'-terminated UTF-8 encoding.
  // Handles allocation internally.
  // Notice that the '\x00' termination is meaningless for strings and regexps
  // which may contain the zero-character, but can be used as terminator for
  // identifiers, numerals and regexp flags.
  LiteralsBuffer literal_buffer_;

  bool stack_overflow_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;
};


// ----------------------------------------------------------------------------
// Scanner::LiteralScope

template <typename InputStream, typename LiteralsBuffer>
Scanner<InputStream, LiteralsBuffer>::LiteralScope::LiteralScope(
    Scanner* self, LiteralType type)
    : scanner_(self), complete_(false) {
  self->StartLiteral(type);
}


template <typename InputStream, typename LiteralsBuffer>
Scanner<InputStream, LiteralsBuffer>::LiteralScope::~LiteralScope() {
  if (!complete_) scanner_->DropLiteral();
}

template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::LiteralScope::Complete() {
  scanner_->TerminateLiteral();
  complete_ = true;
}


// ----------------------------------------------------------------------------
// Scanner.
template <typename InputStream, typename LiteralsBuffer>
Scanner<InputStream, LiteralsBuffer>::Scanner()
    : stack_guard_(kMaxStackSize),
      has_line_terminator_before_next_(false),
      source_(NULL),
      stack_overflow_(false) {}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::Initialize(InputStream* stream) {
  source_ = stream;

  // Initialize current_ to not refer to a literal.
  current_.literal_length = 0;
  // Reset literal buffer.
  literal_buffer_.Reset();

  // Set c0_ (one character ahead)
  ASSERT(kCharacterLookaheadBufferSize == 1);
  Advance();

  // Skip initial whitespace allowing HTML comment ends just like
  // after a newline and scan first token.
  has_line_terminator_before_next_ = true;
  SkipWhiteSpace();
  Scan();
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::Next() {
  // BUG 1215673: Find a thread safe way to set a stack limit in
  // pre-parse mode. Otherwise, we cannot safely pre-parse from other
  // threads.
  current_ = next_;
  // Check for stack-overflow before returning any tokens.
  if (stack_guard_.has_overflowed()) {
    stack_overflow_ = true;
    next_.token = i::Token::ILLEGAL;
  } else {
    has_line_terminator_before_next_ = false;
    Scan();
  }
  return current_.token;
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::StartLiteral(LiteralType type) {
  // Only record string and literal identifiers when preparsing.
  // Those are the ones that are recorded as symbols. Numbers and
  // regexps are not recorded.
  if (type == kLiteralString || type == kLiteralIdentifier) {
    literal_buffer_.StartLiteral();
  }
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::AddLiteralChar(uc32 c) {
  literal_buffer_.AddChar(c);
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::TerminateLiteral() {
  i::Vector<const char> chars = literal_buffer_.EndLiteral();
  next_.literal_chars = chars.start();
  next_.literal_length = chars.length();
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::DropLiteral() {
  literal_buffer_.DropLiteral();
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::AddLiteralCharAdvance() {
  AddLiteralChar(c0_);
  Advance();
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


template <typename InputStream, typename LiteralsBuffer>
bool Scanner<InputStream, LiteralsBuffer>::SkipWhiteSpace() {
  int start_position = source_pos();

  while (true) {
    // We treat byte-order marks (BOMs) as whitespace for better
    // compatibility with Spidermonkey and other JavaScript engines.
    while (i::ScannerConstants::kIsWhiteSpace.get(c0_)
           || IsByteOrderMark(c0_)) {
      // IsWhiteSpace() includes line terminators!
      if (i::ScannerConstants::kIsLineTerminator.get(c0_)) {
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


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::SkipSingleLineComment() {
  Advance();

  // The line terminator at the end of the line is not considered
  // to be part of the single-line comment; it is recognized
  // separately by the lexical grammar and becomes part of the
  // stream of input elements for the syntactic grammar (see
  // ECMA-262, section 7.4, page 12).
  while (c0_ >= 0 && !i::ScannerConstants::kIsLineTerminator.get(c0_)) {
    Advance();
  }

  return i::Token::WHITESPACE;
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::SkipMultiLineComment() {
  ASSERT(c0_ == '*');
  Advance();

  while (c0_ >= 0) {
    char ch = c0_;
    Advance();
    // If we have reached the end of the multi-line comment, we
    // consume the '/' and insert a whitespace. This way all
    // multi-line comments are treated as whitespace - even the ones
    // containing line terminators. This contradicts ECMA-262, section
    // 7.4, page 12, that says that multi-line comments containing
    // line terminators should be treated as a line terminator, but it
    // matches the behaviour of SpiderMonkey and KJS.
    if (ch == '*' && c0_ == '/') {
      c0_ = ' ';
      return i::Token::WHITESPACE;
    }
  }

  // Unterminated multi-line comment.
  return i::Token::ILLEGAL;
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::ScanHtmlComment() {
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
  return i::Token::LT;
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::Scan() {
  next_.literal_length = 0;
  i::Token::Value token;
  do {
    // Remember the position of the next token
    next_.location.beg_pos = source_pos();

    switch (c0_) {
      case ' ':
      case '\t':
        Advance();
        token = i::Token::WHITESPACE;
        break;

      case '\n':
        Advance();
        has_line_terminator_before_next_ = true;
        token = i::Token::WHITESPACE;
        break;

      case '"': case '\'':
        token = ScanString();
        break;

      case '<':
        // < <= << <<= <!--
        Advance();
        if (c0_ == '=') {
          token = Select(i::Token::LTE);
        } else if (c0_ == '<') {
          token = Select('=', i::Token::ASSIGN_SHL, i::Token::SHL);
        } else if (c0_ == '!') {
          token = ScanHtmlComment();
        } else {
          token = i::Token::LT;
        }
        break;

      case '>':
        // > >= >> >>= >>> >>>=
        Advance();
        if (c0_ == '=') {
          token = Select(i::Token::GTE);
        } else if (c0_ == '>') {
          // >> >>= >>> >>>=
          Advance();
          if (c0_ == '=') {
            token = Select(i::Token::ASSIGN_SAR);
          } else if (c0_ == '>') {
            token = Select('=', i::Token::ASSIGN_SHR, i::Token::SHR);
          } else {
            token = i::Token::SAR;
          }
        } else {
          token = i::Token::GT;
        }
        break;

      case '=':
        // = == ===
        Advance();
        if (c0_ == '=') {
          token = Select('=', i::Token::EQ_STRICT, i::Token::EQ);
        } else {
          token = i::Token::ASSIGN;
        }
        break;

      case '!':
        // ! != !==
        Advance();
        if (c0_ == '=') {
          token = Select('=', i::Token::NE_STRICT, i::Token::NE);
        } else {
          token = i::Token::NOT;
        }
        break;

      case '+':
        // + ++ +=
        Advance();
        if (c0_ == '+') {
          token = Select(i::Token::INC);
        } else if (c0_ == '=') {
          token = Select(i::Token::ASSIGN_ADD);
        } else {
          token = i::Token::ADD;
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
            token = i::Token::DEC;
          }
        } else if (c0_ == '=') {
          token = Select(i::Token::ASSIGN_SUB);
        } else {
          token = i::Token::SUB;
        }
        break;

      case '*':
        // * *=
        token = Select('=', i::Token::ASSIGN_MUL, i::Token::MUL);
        break;

      case '%':
        // % %=
        token = Select('=', i::Token::ASSIGN_MOD, i::Token::MOD);
        break;

      case '/':
        // /  // /* /=
        Advance();
        if (c0_ == '/') {
          token = SkipSingleLineComment();
        } else if (c0_ == '*') {
          token = SkipMultiLineComment();
        } else if (c0_ == '=') {
          token = Select(i::Token::ASSIGN_DIV);
        } else {
          token = i::Token::DIV;
        }
        break;

      case '&':
        // & && &=
        Advance();
        if (c0_ == '&') {
          token = Select(i::Token::AND);
        } else if (c0_ == '=') {
          token = Select(i::Token::ASSIGN_BIT_AND);
        } else {
          token = i::Token::BIT_AND;
        }
        break;

      case '|':
        // | || |=
        Advance();
        if (c0_ == '|') {
          token = Select(i::Token::OR);
        } else if (c0_ == '=') {
          token = Select(i::Token::ASSIGN_BIT_OR);
        } else {
          token = i::Token::BIT_OR;
        }
        break;

      case '^':
        // ^ ^=
        token = Select('=', i::Token::ASSIGN_BIT_XOR, i::Token::BIT_XOR);
        break;

      case '.':
        // . Number
        Advance();
        if (i::IsDecimalDigit(c0_)) {
          token = ScanNumber(true);
        } else {
          token = i::Token::PERIOD;
        }
        break;

      case ':':
        token = Select(i::Token::COLON);
        break;

      case ';':
        token = Select(i::Token::SEMICOLON);
        break;

      case ',':
        token = Select(i::Token::COMMA);
        break;

      case '(':
        token = Select(i::Token::LPAREN);
        break;

      case ')':
        token = Select(i::Token::RPAREN);
        break;

      case '[':
        token = Select(i::Token::LBRACK);
        break;

      case ']':
        token = Select(i::Token::RBRACK);
        break;

      case '{':
        token = Select(i::Token::LBRACE);
        break;

      case '}':
        token = Select(i::Token::RBRACE);
        break;

      case '?':
        token = Select(i::Token::CONDITIONAL);
        break;

      case '~':
        token = Select(i::Token::BIT_NOT);
        break;

      default:
        if (i::ScannerConstants::kIsIdentifierStart.get(c0_)) {
          token = ScanIdentifier();
        } else if (i::IsDecimalDigit(c0_)) {
          token = ScanNumber(false);
        } else if (SkipWhiteSpace()) {
          token = i::Token::WHITESPACE;
        } else if (c0_ < 0) {
          token = i::Token::EOS;
        } else {
          token = Select(i::Token::ILLEGAL);
        }
        break;
    }

    // Continue scanning for tokens as long as we're just skipping
    // whitespace.
  } while (token == i::Token::WHITESPACE);

  next_.location.end_pos = source_pos();
  next_.token = token;
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::SeekForward(int pos) {
  source_->SeekForward(pos - 1);
  Advance();
  // This function is only called to seek to the location
  // of the end of a function (at the "}" token). It doesn't matter
  // whether there was a line terminator in the part we skip.
  has_line_terminator_before_next_ = false;
  Scan();
}


template <typename InputStream, typename LiteralsBuffer>
uc32 Scanner<InputStream, LiteralsBuffer>::ScanHexEscape(uc32 c, int length) {
  ASSERT(length <= 4);  // prevent overflow

  uc32 digits[4];
  uc32 x = 0;
  for (int i = 0; i < length; i++) {
    digits[i] = c0_;
    int d = HexValue(c0_);
    if (d < 0) {
      // According to ECMA-262, 3rd, 7.8.4, page 18, these hex escapes
      // should be illegal, but other JS VMs just return the
      // non-escaped version of the original character.

      // Push back digits read, except the last one (in c0_).
      for (int j = i-1; j >= 0; j--) {
        PushBack(digits[j]);
      }
      // Notice: No handling of error - treat it as "\u"->"u".
      return c;
    }
    x = x * 16 + d;
    Advance();
  }

  return x;
}


// Octal escapes of the forms '\0xx' and '\xxx' are not a part of
// ECMA-262. Other JS VMs support them.
template <typename InputStream, typename LiteralsBuffer>
uc32 Scanner<InputStream, LiteralsBuffer>::ScanOctalEscape(
    uc32 c, int length) {
  uc32 x = c - '0';
  for (int i = 0; i < length; i++) {
    int d = c0_ - '0';
    if (d < 0 || d > 7) break;
    int nx = x * 8 + d;
    if (nx >= 256) break;
    x = nx;
    Advance();
  }
  return x;
}


template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::ScanEscape() {
  uc32 c = c0_;
  Advance();

  // Skip escaped newlines.
  if (i::ScannerConstants::kIsLineTerminator.get(c)) {
    // Allow CR+LF newlines in multiline string literals.
    if (i::IsCarriageReturn(c) && i::IsLineFeed(c0_)) Advance();
    // Allow LF+CR newlines in multiline string literals.
    if (i::IsLineFeed(c) && i::IsCarriageReturn(c0_)) Advance();
    return;
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
    case 'u' : c = ScanHexEscape(c, 4); break;
    case 'v' : c = '\v'; break;
    case 'x' : c = ScanHexEscape(c, 2); break;
    case '0' :  // fall through
    case '1' :  // fall through
    case '2' :  // fall through
    case '3' :  // fall through
    case '4' :  // fall through
    case '5' :  // fall through
    case '6' :  // fall through
    case '7' : c = ScanOctalEscape(c, 2); break;
  }

  // According to ECMA-262, 3rd, 7.8.4 (p 18ff) these
  // should be illegal, but they are commonly handled
  // as non-escaped characters by JS VMs.
  AddLiteralChar(c);
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::ScanString() {
  uc32 quote = c0_;
  Advance();  // consume quote

  LiteralScope literal(this, kLiteralString);
  while (c0_ != quote && c0_ >= 0
         && !i::ScannerConstants::kIsLineTerminator.get(c0_)) {
    uc32 c = c0_;
    Advance();
    if (c == '\\') {
      if (c0_ < 0) return i::Token::ILLEGAL;
      ScanEscape();
    } else {
      AddLiteralChar(c);
    }
  }
  if (c0_ != quote) return i::Token::ILLEGAL;
  literal.Complete();

  Advance();  // consume quote
  return i::Token::STRING;
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::Select(
    i::Token::Value tok) {
  Advance();
  return tok;
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::Select(
    uc32 next,
    i::Token::Value then,
    i::Token::Value else_) {
  Advance();
  if (c0_ == next) {
    Advance();
    return then;
  } else {
    return else_;
  }
}


// Returns true if any decimal digits were scanned, returns false otherwise.
template <typename InputStream, typename LiteralsBuffer>
void Scanner<InputStream, LiteralsBuffer>::ScanDecimalDigits() {
  while (i::IsDecimalDigit(c0_))
    AddLiteralCharAdvance();
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::ScanNumber(
    bool seen_period) {
  // c0_ is the first digit of the number or the fraction.
  ASSERT(i::IsDecimalDigit(c0_));

  enum { DECIMAL, HEX, OCTAL } kind = DECIMAL;

  LiteralScope literal(this, kLiteralNumber);
  if (seen_period) {
    // we have already seen a decimal point of the float
    AddLiteralChar('.');
    ScanDecimalDigits();  // we know we have at least one digit

  } else {
    // if the first character is '0' we must check for octals and hex
    if (c0_ == '0') {
      AddLiteralCharAdvance();

      // either 0, 0exxx, 0Exxx, 0.xxx, an octal number, or a hex number
      if (c0_ == 'x' || c0_ == 'X') {
        // hex number
        kind = HEX;
        AddLiteralCharAdvance();
        if (!i::IsHexDigit(c0_)) {
          // we must have at least one hex digit after 'x'/'X'
          return i::Token::ILLEGAL;
        }
        while (i::IsHexDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if ('0' <= c0_ && c0_ <= '7') {
        // (possible) octal number
        kind = OCTAL;
        while (true) {
          if (c0_ == '8' || c0_ == '9') {
            kind = DECIMAL;
            break;
          }
          if (c0_  < '0' || '7'  < c0_) break;
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
    if (kind == OCTAL) return i::Token::ILLEGAL;
    // scan exponent
    AddLiteralCharAdvance();
    if (c0_ == '+' || c0_ == '-')
      AddLiteralCharAdvance();
    if (!i::IsDecimalDigit(c0_)) {
      // we must have at least one decimal digit after 'e'/'E'
      return i::Token::ILLEGAL;
    }
    ScanDecimalDigits();
  }

  // The source character immediately following a numeric literal must
  // not be an identifier start or a decimal digit; see ECMA-262
  // section 7.8.3, page 17 (note that we read only one decimal digit
  // if the value is 0).
  if (i::IsDecimalDigit(c0_)
      || i::ScannerConstants::kIsIdentifierStart.get(c0_))
    return i::Token::ILLEGAL;

  literal.Complete();

  return i::Token::NUMBER;
}


template <typename InputStream, typename LiteralsBuffer>
uc32 Scanner<InputStream, LiteralsBuffer>::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return unibrow::Utf8::kBadChar;
  Advance();
  uc32 c = ScanHexEscape('u', 4);
  // We do not allow a unicode escape sequence to start another
  // unicode escape sequence.
  if (c == '\\') return unibrow::Utf8::kBadChar;
  return c;
}


template <typename InputStream, typename LiteralsBuffer>
i::Token::Value Scanner<InputStream, LiteralsBuffer>::ScanIdentifier() {
  ASSERT(i::ScannerConstants::kIsIdentifierStart.get(c0_));

  LiteralScope literal(this, kLiteralIdentifier);
  i::KeywordMatcher keyword_match;

  // Scan identifier start character.
  if (c0_ == '\\') {
    uc32 c = ScanIdentifierUnicodeEscape();
    // Only allow legal identifier start characters.
    if (!i::ScannerConstants::kIsIdentifierStart.get(c)) {
      return i::Token::ILLEGAL;
    }
    AddLiteralChar(c);
    keyword_match.Fail();
  } else {
    AddLiteralChar(c0_);
    keyword_match.AddChar(c0_);
    Advance();
  }

  // Scan the rest of the identifier characters.
  while (i::ScannerConstants::kIsIdentifierPart.get(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      // Only allow legal identifier part characters.
      if (!i::ScannerConstants::kIsIdentifierPart.get(c)) {
        return i::Token::ILLEGAL;
      }
      AddLiteralChar(c);
      keyword_match.Fail();
    } else {
      AddLiteralChar(c0_);
      keyword_match.AddChar(c0_);
      Advance();
    }
  }
  literal.Complete();

  return keyword_match.token();
}


template <typename InputStream, typename LiteralsBuffer>
bool Scanner<InputStream, LiteralsBuffer>::ScanRegExpPattern(bool seen_equal) {
  // Scan: ('/' | '/=') RegularExpressionBody '/' RegularExpressionFlags
  bool in_character_class = false;

  // Previous token is either '/' or '/=', in the second case, the
  // pattern starts at =.
  next_.location.beg_pos = source_pos() - (seen_equal ? 2 : 1);
  next_.location.end_pos = source_pos() - (seen_equal ? 1 : 0);

  // Scan regular expression body: According to ECMA-262, 3rd, 7.8.5,
  // the scanner should pass uninterpreted bodies to the RegExp
  // constructor.
  LiteralScope literal(this, kLiteralRegExp);
  if (seen_equal)
    AddLiteralChar('=');

  while (c0_ != '/' || in_character_class) {
    if (i::ScannerConstants::kIsLineTerminator.get(c0_) || c0_ < 0) {
      return false;
    }
    if (c0_ == '\\') {  // escaped character
      AddLiteralCharAdvance();
      if (i::ScannerConstants::kIsLineTerminator.get(c0_) || c0_ < 0) {
        return false;
      }
      AddLiteralCharAdvance();
    } else {  // unescaped character
      if (c0_ == '[') in_character_class = true;
      if (c0_ == ']') in_character_class = false;
      AddLiteralCharAdvance();
    }
  }
  Advance();  // consume '/'

  literal.Complete();

  return true;
}

template <typename InputStream, typename LiteralsBuffer>
bool Scanner<InputStream, LiteralsBuffer>::ScanRegExpFlags() {
  // Scan regular expression flags.
  LiteralScope literal(this, kLiteralRegExpFlags);
  while (i::ScannerConstants::kIsIdentifierPart.get(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      if (c != static_cast<uc32>(unibrow::Utf8::kBadChar)) {
        // We allow any escaped character, unlike the restriction on
        // IdentifierPart when it is used to build an IdentifierName.
        AddLiteralChar(c);
        continue;
      }
    }
    AddLiteralCharAdvance();
  }
  literal.Complete();

  next_.location.end_pos = source_pos() - 1;
  return true;
}


} }  // namespace v8::preparser

#endif  // V8_PRESCANNER_H_
