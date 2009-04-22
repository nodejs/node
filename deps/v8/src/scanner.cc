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

#include "v8.h"

#include "ast.h"
#include "scanner.h"

namespace v8 { namespace internal {

// ----------------------------------------------------------------------------
// Character predicates


unibrow::Predicate<IdentifierStart, 128> Scanner::kIsIdentifierStart;
unibrow::Predicate<IdentifierPart, 128> Scanner::kIsIdentifierPart;
unibrow::Predicate<unibrow::LineTerminator, 128> Scanner::kIsLineTerminator;
unibrow::Predicate<unibrow::WhiteSpace, 128> Scanner::kIsWhiteSpace;


StaticResource<Scanner::Utf8Decoder> Scanner::utf8_decoder_;


// ----------------------------------------------------------------------------
// UTF8Buffer

UTF8Buffer::UTF8Buffer() : data_(NULL) {
  Initialize(NULL, 0);
}


UTF8Buffer::~UTF8Buffer() {
  DeleteArray(data_);
}


void UTF8Buffer::Initialize(char* src, int length) {
  DeleteArray(data_);
  data_ = src;
  size_ = length;
  Reset();
}


void UTF8Buffer::AddChar(uc32 c) {
  const int min_size = 1024;
  if (pos_ + static_cast<int>(unibrow::Utf8::kMaxEncodedSize) > size_) {
    int new_size = size_ * 2;
    if (new_size < min_size) {
      new_size = min_size;
    }
    char* new_data = NewArray<char>(new_size);
    memcpy(new_data, data_, pos_);
    DeleteArray(data_);
    data_ = new_data;
    size_ = new_size;
  }
  if (static_cast<unsigned>(c) < unibrow::Utf8::kMaxOneByteChar) {
    data_[pos_++] = c;  // common case: 7bit ASCII
  } else {
    pos_ += unibrow::Utf8::Encode(&data_[pos_], c);
  }
  ASSERT(pos_ <= size_);
}


// ----------------------------------------------------------------------------
// UTF16Buffer


UTF16Buffer::UTF16Buffer()
  : pos_(0),
    pushback_buffer_(0),
    last_(0),
    stream_(NULL) { }


void UTF16Buffer::Initialize(Handle<String> data,
                             unibrow::CharacterStream* input) {
  data_ = data;
  pos_ = 0;
  stream_ = input;
}


Handle<String> UTF16Buffer::SubString(int start, int end) {
  return internal::SubString(data_, start, end);
}


void UTF16Buffer::PushBack(uc32 ch) {
  pushback_buffer()->Add(last_);
  last_ = ch;
  pos_--;
}


uc32 UTF16Buffer::Advance() {
  // NOTE: It is of importance to Persian / Farsi resources that we do
  // *not* strip format control characters in the scanner; see
  //
  //    https://bugzilla.mozilla.org/show_bug.cgi?id=274152
  //
  // So, even though ECMA-262, section 7.1, page 11, dictates that we
  // must remove Unicode format-control characters, we do not. This is
  // in line with how IE and SpiderMonkey handles it.
  if (!pushback_buffer()->is_empty()) {
    pos_++;
    return last_ = pushback_buffer()->RemoveLast();
  } else if (stream_->has_more()) {
    pos_++;
    uc32 next = stream_->GetNext();
    return last_ = next;
  } else {
    // note: currently the following increment is necessary to avoid a
    // test-parser problem!
    pos_++;
    return last_ = static_cast<uc32>(-1);
  }
}


void UTF16Buffer::SeekForward(int pos) {
  pos_ = pos;
  ASSERT(pushback_buffer()->is_empty());
  stream_->Seek(pos);
}


// ----------------------------------------------------------------------------
// Scanner

Scanner::Scanner(bool pre) : stack_overflow_(false), is_pre_parsing_(pre) {
  Token::Initialize();
}


void Scanner::Init(Handle<String> source, unibrow::CharacterStream* stream,
    int position) {
  // Initialize the source buffer.
  source_.Initialize(source, stream);
  position_ = position;

  // Reset literals buffer
  literals_.Reset();

  // Set c0_ (one character ahead)
  ASSERT(kCharacterLookaheadBufferSize == 1);
  Advance();

  // Skip initial whitespace (allowing HTML comment ends) and scan
  // first token.
  SkipWhiteSpace(true);
  Scan();
}


Handle<String> Scanner::SubString(int start, int end) {
  return source_.SubString(start - position_, end - position_);
}


Token::Value Scanner::Next() {
  // BUG 1215673: Find a thread safe way to set a stack limit in
  // pre-parse mode. Otherwise, we cannot safely pre-parse from other
  // threads.
  current_ = next_;
  // Check for stack-overflow before returning any tokens.
  StackLimitCheck check;
  if (check.HasOverflowed()) {
    stack_overflow_ = true;
    next_.token = Token::ILLEGAL;
  } else {
    Scan();
  }
  return current_.token;
}


void Scanner::StartLiteral() {
  next_.literal_pos = literals_.pos();
}


void Scanner::AddChar(uc32 c) {
  literals_.AddChar(c);
}


void Scanner::TerminateLiteral() {
  next_.literal_end = literals_.pos();
  AddChar(0);
}


void Scanner::AddCharAdvance() {
  AddChar(c0_);
  Advance();
}


void Scanner::Advance() {
  c0_ = source_.Advance();
}


void Scanner::PushBack(uc32 ch) {
  source_.PushBack(ch);
  c0_ = ch;
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


void Scanner::SkipWhiteSpace(bool initial) {
  has_line_terminator_before_next_ = initial;

  while (true) {
    // We treat byte-order marks (BOMs) as whitespace for better
    // compatibility with Spidermonkey and other JavaScript engines.
    while (kIsWhiteSpace.get(c0_) || IsByteOrderMark(c0_)) {
      // IsWhiteSpace() includes line terminators!
      if (kIsLineTerminator.get(c0_))
        // Ignore line terminators, but remember them. This is necessary
        // for automatic semicolon insertion.
        has_line_terminator_before_next_ = true;
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
    return;
  }
}


Token::Value Scanner::SkipSingleLineComment() {
  Advance();

  // The line terminator at the end of the line is not considered
  // to be part of the single-line comment; it is recognized
  // separately by the lexical grammar and becomes part of the
  // stream of input elements for the syntactic grammar (see
  // ECMA-262, section 7.4, page 12).
  while (c0_ >= 0 && !kIsLineTerminator.get(c0_)) {
    Advance();
  }

  return Token::COMMENT;
}


Token::Value Scanner::SkipMultiLineComment() {
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
      return Token::COMMENT;
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
  Token::Value token;
  bool has_line_terminator = false;
  do {
    SkipWhiteSpace(has_line_terminator);

    // Remember the line terminator in previous loop
    has_line_terminator = has_line_terminator_before_next();

    // Remember the position of the next token
    next_.location.beg_pos = source_pos();

    token = ScanToken();
  } while (token == Token::COMMENT);

  next_.location.end_pos = source_pos();
  next_.token = token;
}


void Scanner::SeekForward(int pos) {
  source_.SeekForward(pos - 1);
  Advance();
  Scan();
}


uc32 Scanner::ScanHexEscape(uc32 c, int length) {
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
uc32 Scanner::ScanOctalEscape(uc32 c, int length) {
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


void Scanner::ScanEscape() {
  uc32 c = c0_;
  Advance();

  // Skip escaped newlines.
  if (kIsLineTerminator.get(c)) {
    // Allow CR+LF newlines in multiline string literals.
    if (IsCarriageReturn(c) && IsLineFeed(c0_)) Advance();
    // Allow LF+CR newlines in multiline string literals.
    if (IsLineFeed(c) && IsCarriageReturn(c0_)) Advance();
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
  AddChar(c);
}


Token::Value Scanner::ScanString() {
  uc32 quote = c0_;
  Advance();  // consume quote

  StartLiteral();
  while (c0_ != quote && c0_ >= 0 && !kIsLineTerminator.get(c0_)) {
    uc32 c = c0_;
    Advance();
    if (c == '\\') {
      if (c0_ < 0) return Token::ILLEGAL;
      ScanEscape();
    } else {
      AddChar(c);
    }
  }
  if (c0_ != quote) {
    return Token::ILLEGAL;
  }
  TerminateLiteral();

  Advance();  // consume quote
  return Token::STRING;
}


Token::Value Scanner::Select(Token::Value tok) {
  Advance();
  return tok;
}


Token::Value Scanner::Select(uc32 next, Token::Value then, Token::Value else_) {
  Advance();
  if (c0_ == next) {
    Advance();
    return then;
  } else {
    return else_;
  }
}


Token::Value Scanner::ScanToken() {
  switch (c0_) {
    // strings
    case '"': case '\'':
      return ScanString();

    case '<':
      // < <= << <<= <!--
      Advance();
      if (c0_ == '=') return Select(Token::LTE);
      if (c0_ == '<') return Select('=', Token::ASSIGN_SHL, Token::SHL);
      if (c0_ == '!') return ScanHtmlComment();
      return Token::LT;

    case '>':
      // > >= >> >>= >>> >>>=
      Advance();
      if (c0_ == '=') return Select(Token::GTE);
      if (c0_ == '>') {
        // >> >>= >>> >>>=
        Advance();
        if (c0_ == '=') return Select(Token::ASSIGN_SAR);
        if (c0_ == '>') return Select('=', Token::ASSIGN_SHR, Token::SHR);
        return Token::SAR;
      }
      return Token::GT;

    case '=':
      // = == ===
      Advance();
      if (c0_ == '=') return Select('=', Token::EQ_STRICT, Token::EQ);
      return Token::ASSIGN;

    case '!':
      // ! != !==
      Advance();
      if (c0_ == '=') return Select('=', Token::NE_STRICT, Token::NE);
      return Token::NOT;

    case '+':
      // + ++ +=
      Advance();
      if (c0_ == '+') return Select(Token::INC);
      if (c0_ == '=') return Select(Token::ASSIGN_ADD);
      return Token::ADD;

    case '-':
      // - -- -=
      Advance();
      if (c0_ == '-') return Select(Token::DEC);
      if (c0_ == '=') return Select(Token::ASSIGN_SUB);
      return Token::SUB;

    case '*':
      // * *=
      return Select('=', Token::ASSIGN_MUL, Token::MUL);

    case '%':
      // % %=
      return Select('=', Token::ASSIGN_MOD, Token::MOD);

    case '/':
      // /  // /* /=
      Advance();
      if (c0_ == '/') return SkipSingleLineComment();
      if (c0_ == '*') return SkipMultiLineComment();
      if (c0_ == '=') return Select(Token::ASSIGN_DIV);
      return Token::DIV;

    case '&':
      // & && &=
      Advance();
      if (c0_ == '&') return Select(Token::AND);
      if (c0_ == '=') return Select(Token::ASSIGN_BIT_AND);
      return Token::BIT_AND;

    case '|':
      // | || |=
      Advance();
      if (c0_ == '|') return Select(Token::OR);
      if (c0_ == '=') return Select(Token::ASSIGN_BIT_OR);
      return Token::BIT_OR;

    case '^':
      // ^ ^=
      return Select('=', Token::ASSIGN_BIT_XOR, Token::BIT_XOR);

    case '.':
      // . Number
      Advance();
      if (IsDecimalDigit(c0_)) return ScanNumber(true);
      return Token::PERIOD;

    case ':':
      return Select(Token::COLON);

    case ';':
      return Select(Token::SEMICOLON);

    case ',':
      return Select(Token::COMMA);

    case '(':
      return Select(Token::LPAREN);

    case ')':
      return Select(Token::RPAREN);

    case '[':
      return Select(Token::LBRACK);

    case ']':
      return Select(Token::RBRACK);

    case '{':
      return Select(Token::LBRACE);

    case '}':
      return Select(Token::RBRACE);

    case '?':
      return Select(Token::CONDITIONAL);

    case '~':
      return Select(Token::BIT_NOT);

    default:
      if (kIsIdentifierStart.get(c0_))
        return ScanIdentifier();
      if (IsDecimalDigit(c0_))
        return ScanNumber(false);
      if (c0_ < 0)
        return Token::EOS;
      return Select(Token::ILLEGAL);
  }

  UNREACHABLE();
  return Token::ILLEGAL;
}


// Returns true if any decimal digits were scanned, returns false otherwise.
void Scanner::ScanDecimalDigits() {
  while (IsDecimalDigit(c0_))
    AddCharAdvance();
}


Token::Value Scanner::ScanNumber(bool seen_period) {
  ASSERT(IsDecimalDigit(c0_));  // the first digit of the number or the fraction

  enum { DECIMAL, HEX, OCTAL } kind = DECIMAL;

  StartLiteral();
  if (seen_period) {
    // we have already seen a decimal point of the float
    AddChar('.');
    ScanDecimalDigits();  // we know we have at least one digit

  } else {
    // if the first character is '0' we must check for octals and hex
    if (c0_ == '0') {
      AddCharAdvance();

      // either 0, 0exxx, 0Exxx, 0.xxx, an octal number, or a hex number
      if (c0_ == 'x' || c0_ == 'X') {
        // hex number
        kind = HEX;
        AddCharAdvance();
        if (!IsHexDigit(c0_))
          // we must have at least one hex digit after 'x'/'X'
          return Token::ILLEGAL;
        while (IsHexDigit(c0_))
          AddCharAdvance();

      } else if ('0' <= c0_ && c0_ <= '7') {
        // (possible) octal number
        kind = OCTAL;
        while (true) {
          if (c0_ == '8' || c0_ == '9') {
            kind = DECIMAL;
            break;
          }
          if (c0_  < '0' || '7'  < c0_) break;
          AddCharAdvance();
        }
      }
    }

    // Parse decimal digits and allow trailing fractional part.
    if (kind == DECIMAL) {
      ScanDecimalDigits();  // optional
      if (c0_ == '.') {
        AddCharAdvance();
        ScanDecimalDigits();  // optional
      }
    }
  }

  // scan exponent, if any
  if (c0_ == 'e' || c0_ == 'E') {
    ASSERT(kind != HEX);  // 'e'/'E' must be scanned as part of the hex number
    if (kind == OCTAL) return Token::ILLEGAL;  // no exponent for octals allowed
    // scan exponent
    AddCharAdvance();
    if (c0_ == '+' || c0_ == '-')
      AddCharAdvance();
    if (!IsDecimalDigit(c0_))
      // we must have at least one decimal digit after 'e'/'E'
      return Token::ILLEGAL;
    ScanDecimalDigits();
  }
  TerminateLiteral();

  // The source character immediately following a numeric literal must
  // not be an identifier start or a decimal digit; see ECMA-262
  // section 7.8.3, page 17 (note that we read only one decimal digit
  // if the value is 0).
  if (IsDecimalDigit(c0_) || kIsIdentifierStart.get(c0_))
    return Token::ILLEGAL;

  return Token::NUMBER;
}


uc32 Scanner::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return unibrow::Utf8::kBadChar;
  Advance();
  uc32 c = ScanHexEscape('u', 4);
  // We do not allow a unicode escape sequence to start another
  // unicode escape sequence.
  if (c == '\\') return unibrow::Utf8::kBadChar;
  return c;
}


Token::Value Scanner::ScanIdentifier() {
  ASSERT(kIsIdentifierStart.get(c0_));

  bool has_escapes = false;

  StartLiteral();
  // Scan identifier start character.
  if (c0_ == '\\') {
    has_escapes = true;
    uc32 c = ScanIdentifierUnicodeEscape();
    // Only allow legal identifier start characters.
    if (!kIsIdentifierStart.get(c)) return Token::ILLEGAL;
    AddChar(c);
  } else {
    AddCharAdvance();
  }
  // Scan the rest of the identifier characters.
  while (kIsIdentifierPart.get(c0_)) {
    if (c0_ == '\\') {
      has_escapes = true;
      uc32 c = ScanIdentifierUnicodeEscape();
      // Only allow legal identifier part characters.
      if (!kIsIdentifierPart.get(c)) return Token::ILLEGAL;
      AddChar(c);
    } else {
      AddCharAdvance();
    }
  }
  TerminateLiteral();

  // We don't have any 1-letter keywords (this is probably a common case).
  if ((next_.literal_end - next_.literal_pos) == 1)
    return Token::IDENTIFIER;

  // If the identifier contains unicode escapes, it must not be
  // resolved to a keyword.
  if (has_escapes)
    return Token::IDENTIFIER;

  return Token::Lookup(&literals_.data()[next_.literal_pos]);
}



bool Scanner::IsIdentifier(unibrow::CharacterStream* buffer) {
  // Checks whether the buffer contains an identifier (no escape).
  if (!buffer->has_more()) return false;
  if (!kIsIdentifierStart.get(buffer->GetNext())) return false;
  while (buffer->has_more()) {
    if (!kIsIdentifierPart.get(buffer->GetNext())) return false;
  }
  return true;
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
  StartLiteral();
  if (seen_equal)
    AddChar('=');

  while (c0_ != '/' || in_character_class) {
    if (kIsLineTerminator.get(c0_) || c0_ < 0)
      return false;
    if (c0_ == '\\') {  // escaped character
      AddCharAdvance();
      if (kIsLineTerminator.get(c0_) || c0_ < 0)
        return false;
      AddCharAdvance();
    } else {  // unescaped character
      if (c0_ == '[')
        in_character_class = true;
      if (c0_ == ']')
        in_character_class = false;
      AddCharAdvance();
    }
  }
  Advance();  // consume '/'

  TerminateLiteral();

  return true;
}

bool Scanner::ScanRegExpFlags() {
  // Scan regular expression flags.
  StartLiteral();
  while (kIsIdentifierPart.get(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      if (c != static_cast<uc32>(unibrow::Utf8::kBadChar)) {
        // We allow any escaped character, unlike the restriction on
        // IdentifierPart when it is used to build an IdentifierName.
        AddChar(c);
        continue;
      }
    }
    AddCharAdvance();
  }
  TerminateLiteral();

  next_.location.end_pos = source_pos() - 1;
  return true;
}

} }  // namespace v8::internal
