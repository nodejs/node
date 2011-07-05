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

#include "v8.h"

#include "ast.h"
#include "handles.h"
#include "scanner.h"
#include "unicode-inl.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// BufferedUC16CharacterStreams

BufferedUC16CharacterStream::BufferedUC16CharacterStream()
    : UC16CharacterStream(),
      pushback_limit_(NULL) {
  // Initialize buffer as being empty. First read will fill the buffer.
  buffer_cursor_ = buffer_;
  buffer_end_ = buffer_;
}

BufferedUC16CharacterStream::~BufferedUC16CharacterStream() { }

void BufferedUC16CharacterStream::PushBack(uc32 character) {
  if (character == kEndOfInput) {
    pos_--;
    return;
  }
  if (pushback_limit_ == NULL && buffer_cursor_ > buffer_) {
    // buffer_ is writable, buffer_cursor_ is const pointer.
    buffer_[--buffer_cursor_ - buffer_] = static_cast<uc16>(character);
    pos_--;
    return;
  }
  SlowPushBack(static_cast<uc16>(character));
}


void BufferedUC16CharacterStream::SlowPushBack(uc16 character) {
  // In pushback mode, the end of the buffer contains pushback,
  // and the start of the buffer (from buffer start to pushback_limit_)
  // contains valid data that comes just after the pushback.
  // We NULL the pushback_limit_ if pushing all the way back to the
  // start of the buffer.

  if (pushback_limit_ == NULL) {
    // Enter pushback mode.
    pushback_limit_ = buffer_end_;
    buffer_end_ = buffer_ + kBufferSize;
    buffer_cursor_ = buffer_end_;
  }
  // Ensure that there is room for at least one pushback.
  ASSERT(buffer_cursor_ > buffer_);
  ASSERT(pos_ > 0);
  buffer_[--buffer_cursor_ - buffer_] = character;
  if (buffer_cursor_ == buffer_) {
    pushback_limit_ = NULL;
  } else if (buffer_cursor_ < pushback_limit_) {
    pushback_limit_ = buffer_cursor_;
  }
  pos_--;
}


bool BufferedUC16CharacterStream::ReadBlock() {
  buffer_cursor_ = buffer_;
  if (pushback_limit_ != NULL) {
    // Leave pushback mode.
    buffer_end_ = pushback_limit_;
    pushback_limit_ = NULL;
    // If there were any valid characters left at the
    // start of the buffer, use those.
    if (buffer_cursor_ < buffer_end_) return true;
    // Otherwise read a new block.
  }
  unsigned length = FillBuffer(pos_, kBufferSize);
  buffer_end_ = buffer_ + length;
  return length > 0;
}


unsigned BufferedUC16CharacterStream::SlowSeekForward(unsigned delta) {
  // Leave pushback mode (i.e., ignore that there might be valid data
  // in the buffer before the pushback_limit_ point).
  pushback_limit_ = NULL;
  return BufferSeekForward(delta);
}

// ----------------------------------------------------------------------------
// GenericStringUC16CharacterStream


GenericStringUC16CharacterStream::GenericStringUC16CharacterStream(
    Handle<String> data,
    unsigned start_position,
    unsigned end_position)
    : string_(data),
      length_(end_position) {
  ASSERT(end_position >= start_position);
  buffer_cursor_ = buffer_;
  buffer_end_ = buffer_;
  pos_ = start_position;
}


GenericStringUC16CharacterStream::~GenericStringUC16CharacterStream() { }


unsigned GenericStringUC16CharacterStream::BufferSeekForward(unsigned delta) {
  unsigned old_pos = pos_;
  pos_ = Min(pos_ + delta, length_);
  ReadBlock();
  return pos_ - old_pos;
}


unsigned GenericStringUC16CharacterStream::FillBuffer(unsigned from_pos,
                                                      unsigned length) {
  if (from_pos >= length_) return 0;
  if (from_pos + length > length_) {
    length = length_ - from_pos;
  }
  String::WriteToFlat<uc16>(*string_, buffer_, from_pos, from_pos + length);
  return length;
}


// ----------------------------------------------------------------------------
// Utf8ToUC16CharacterStream
Utf8ToUC16CharacterStream::Utf8ToUC16CharacterStream(const byte* data,
                                                     unsigned length)
    : BufferedUC16CharacterStream(),
      raw_data_(data),
      raw_data_length_(length),
      raw_data_pos_(0),
      raw_character_position_(0) {
  ReadBlock();
}


Utf8ToUC16CharacterStream::~Utf8ToUC16CharacterStream() { }


unsigned Utf8ToUC16CharacterStream::BufferSeekForward(unsigned delta) {
  unsigned old_pos = pos_;
  unsigned target_pos = pos_ + delta;
  SetRawPosition(target_pos);
  pos_ = raw_character_position_;
  ReadBlock();
  return pos_ - old_pos;
}


unsigned Utf8ToUC16CharacterStream::FillBuffer(unsigned char_position,
                                               unsigned length) {
  static const unibrow::uchar kMaxUC16Character = 0xffff;
  SetRawPosition(char_position);
  if (raw_character_position_ != char_position) {
    // char_position was not a valid position in the stream (hit the end
    // while spooling to it).
    return 0u;
  }
  unsigned i = 0;
  while (i < length) {
    if (raw_data_pos_ == raw_data_length_) break;
    unibrow::uchar c = raw_data_[raw_data_pos_];
    if (c <= unibrow::Utf8::kMaxOneByteChar) {
      raw_data_pos_++;
    } else {
      c =  unibrow::Utf8::CalculateValue(raw_data_ + raw_data_pos_,
                                         raw_data_length_ - raw_data_pos_,
                                         &raw_data_pos_);
      // Don't allow characters outside of the BMP.
      if (c > kMaxUC16Character) {
        c = unibrow::Utf8::kBadChar;
      }
    }
    buffer_[i++] = static_cast<uc16>(c);
  }
  raw_character_position_ = char_position + i;
  return i;
}


static const byte kUtf8MultiByteMask = 0xC0;
static const byte kUtf8MultiByteCharStart = 0xC0;
static const byte kUtf8MultiByteCharFollower = 0x80;


#ifdef DEBUG
static bool IsUtf8MultiCharacterStart(byte first_byte) {
  return (first_byte & kUtf8MultiByteMask) == kUtf8MultiByteCharStart;
}
#endif


static bool IsUtf8MultiCharacterFollower(byte later_byte) {
  return (later_byte & kUtf8MultiByteMask) == kUtf8MultiByteCharFollower;
}


// Move the cursor back to point at the preceding UTF-8 character start
// in the buffer.
static inline void Utf8CharacterBack(const byte* buffer, unsigned* cursor) {
  byte character = buffer[--*cursor];
  if (character > unibrow::Utf8::kMaxOneByteChar) {
    ASSERT(IsUtf8MultiCharacterFollower(character));
    // Last byte of a multi-byte character encoding. Step backwards until
    // pointing to the first byte of the encoding, recognized by having the
    // top two bits set.
    while (IsUtf8MultiCharacterFollower(buffer[--*cursor])) { }
    ASSERT(IsUtf8MultiCharacterStart(buffer[*cursor]));
  }
}


// Move the cursor forward to point at the next following UTF-8 character start
// in the buffer.
static inline void Utf8CharacterForward(const byte* buffer, unsigned* cursor) {
  byte character = buffer[(*cursor)++];
  if (character > unibrow::Utf8::kMaxOneByteChar) {
    // First character of a multi-byte character encoding.
    // The number of most-significant one-bits determines the length of the
    // encoding:
    //  110..... - (0xCx, 0xDx) one additional byte (minimum).
    //  1110.... - (0xEx) two additional bytes.
    //  11110... - (0xFx) three additional bytes (maximum).
    ASSERT(IsUtf8MultiCharacterStart(character));
    // Additional bytes is:
    // 1 if value in range 0xC0 .. 0xDF.
    // 2 if value in range 0xE0 .. 0xEF.
    // 3 if value in range 0xF0 .. 0xF7.
    // Encode that in a single value.
    unsigned additional_bytes =
        ((0x3211u) >> (((character - 0xC0) >> 2) & 0xC)) & 0x03;
    *cursor += additional_bytes;
    ASSERT(!IsUtf8MultiCharacterFollower(buffer[1 + additional_bytes]));
  }
}


void Utf8ToUC16CharacterStream::SetRawPosition(unsigned target_position) {
  if (raw_character_position_ > target_position) {
    // Spool backwards in utf8 buffer.
    do {
      Utf8CharacterBack(raw_data_, &raw_data_pos_);
      raw_character_position_--;
    } while (raw_character_position_ > target_position);
    return;
  }
  // Spool forwards in the utf8 buffer.
  while (raw_character_position_ < target_position) {
    if (raw_data_pos_ == raw_data_length_) return;
    Utf8CharacterForward(raw_data_, &raw_data_pos_);
    raw_character_position_++;
  }
}


// ----------------------------------------------------------------------------
// ExternalTwoByteStringUC16CharacterStream

ExternalTwoByteStringUC16CharacterStream::
    ~ExternalTwoByteStringUC16CharacterStream() { }


ExternalTwoByteStringUC16CharacterStream
    ::ExternalTwoByteStringUC16CharacterStream(
        Handle<ExternalTwoByteString> data,
        int start_position,
        int end_position)
    : UC16CharacterStream(),
      source_(data),
      raw_data_(data->GetTwoByteData(start_position)) {
  buffer_cursor_ = raw_data_,
  buffer_end_ = raw_data_ + (end_position - start_position);
  pos_ = start_position;
}


// ----------------------------------------------------------------------------
// Scanner::LiteralScope

Scanner::LiteralScope::LiteralScope(Scanner* self)
    : scanner_(self), complete_(false) {
  self->StartLiteral();
}


Scanner::LiteralScope::~LiteralScope() {
  if (!complete_) scanner_->DropLiteral();
}


void Scanner::LiteralScope::Complete() {
  scanner_->TerminateLiteral();
  complete_ = true;
}


// ----------------------------------------------------------------------------
// V8JavaScriptScanner

V8JavaScriptScanner::V8JavaScriptScanner() : JavaScriptScanner() { }


void V8JavaScriptScanner::Initialize(UC16CharacterStream* source) {
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


// ----------------------------------------------------------------------------
// JsonScanner

JsonScanner::JsonScanner() : Scanner() { }


void JsonScanner::Initialize(UC16CharacterStream* source) {
  source_ = source;
  Init();
  // Skip initial whitespace.
  SkipJsonWhiteSpace();
  // Preload first token as look-ahead.
  ScanJson();
}


Token::Value JsonScanner::Next() {
  // BUG 1215673: Find a thread safe way to set a stack limit in
  // pre-parse mode. Otherwise, we cannot safely pre-parse from other
  // threads.
  current_ = next_;
  // Check for stack-overflow before returning any tokens.
  ScanJson();
  return current_.token;
}


bool JsonScanner::SkipJsonWhiteSpace() {
  int start_position = source_pos();
  // JSON WhiteSpace is tab, carrige-return, newline and space.
  while (c0_ == ' ' || c0_ == '\n' || c0_ == '\r' || c0_ == '\t') {
    Advance();
  }
  return source_pos() != start_position;
}


void JsonScanner::ScanJson() {
  next_.literal_chars = NULL;
  Token::Value token;
  do {
    // Remember the position of the next token
    next_.location.beg_pos = source_pos();
    switch (c0_) {
      case '\t':
      case '\r':
      case '\n':
      case ' ':
        Advance();
        token = Token::WHITESPACE;
        break;
      case '{':
        Advance();
        token = Token::LBRACE;
        break;
      case '}':
        Advance();
        token = Token::RBRACE;
        break;
      case '[':
        Advance();
        token = Token::LBRACK;
        break;
      case ']':
        Advance();
        token = Token::RBRACK;
        break;
      case ':':
        Advance();
        token = Token::COLON;
        break;
      case ',':
        Advance();
        token = Token::COMMA;
        break;
      case '"':
        token = ScanJsonString();
        break;
      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        token = ScanJsonNumber();
        break;
      case 't':
        token = ScanJsonIdentifier("true", Token::TRUE_LITERAL);
        break;
      case 'f':
        token = ScanJsonIdentifier("false", Token::FALSE_LITERAL);
        break;
      case 'n':
        token = ScanJsonIdentifier("null", Token::NULL_LITERAL);
        break;
      default:
        if (c0_ < 0) {
          Advance();
          token = Token::EOS;
        } else {
          Advance();
          token = Select(Token::ILLEGAL);
        }
    }
  } while (token == Token::WHITESPACE);

  next_.location.end_pos = source_pos();
  next_.token = token;
}


Token::Value JsonScanner::ScanJsonString() {
  ASSERT_EQ('"', c0_);
  Advance();
  LiteralScope literal(this);
  while (c0_ != '"') {
    // Check for control character (0x00-0x1f) or unterminated string (<0).
    if (c0_ < 0x20) return Token::ILLEGAL;
    if (c0_ != '\\') {
      AddLiteralCharAdvance();
    } else {
      Advance();
      switch (c0_) {
        case '"':
        case '\\':
        case '/':
          AddLiteralChar(c0_);
          break;
        case 'b':
          AddLiteralChar('\x08');
          break;
        case 'f':
          AddLiteralChar('\x0c');
          break;
        case 'n':
          AddLiteralChar('\x0a');
          break;
        case 'r':
          AddLiteralChar('\x0d');
          break;
        case 't':
          AddLiteralChar('\x09');
          break;
        case 'u': {
          uc32 value = 0;
          for (int i = 0; i < 4; i++) {
            Advance();
            int digit = HexValue(c0_);
            if (digit < 0) {
              return Token::ILLEGAL;
            }
            value = value * 16 + digit;
          }
          AddLiteralChar(value);
          break;
        }
        default:
          return Token::ILLEGAL;
      }
      Advance();
    }
  }
  literal.Complete();
  Advance();
  return Token::STRING;
}


Token::Value JsonScanner::ScanJsonNumber() {
  LiteralScope literal(this);
  bool negative = false;

  if (c0_ == '-') {
    AddLiteralCharAdvance();
    negative = true;
  }
  if (c0_ == '0') {
    AddLiteralCharAdvance();
    // Prefix zero is only allowed if it's the only digit before
    // a decimal point or exponent.
    if ('0' <= c0_ && c0_ <= '9') return Token::ILLEGAL;
  } else {
    int i = 0;
    int digits = 0;
    if (c0_ < '1' || c0_ > '9') return Token::ILLEGAL;
    do {
      i = i * 10 + c0_ - '0';
      digits++;
      AddLiteralCharAdvance();
    } while (c0_ >= '0' && c0_ <= '9');
    if (c0_ != '.' && c0_ != 'e' && c0_ != 'E' && digits < 10) {
      number_ = (negative ? -i : i);
      return Token::NUMBER;
    }
  }
  if (c0_ == '.') {
    AddLiteralCharAdvance();
    if (c0_ < '0' || c0_ > '9') return Token::ILLEGAL;
    do {
      AddLiteralCharAdvance();
    } while (c0_ >= '0' && c0_ <= '9');
  }
  if (AsciiAlphaToLower(c0_) == 'e') {
    AddLiteralCharAdvance();
    if (c0_ == '-' || c0_ == '+') AddLiteralCharAdvance();
    if (c0_ < '0' || c0_ > '9') return Token::ILLEGAL;
    do {
      AddLiteralCharAdvance();
    } while (c0_ >= '0' && c0_ <= '9');
  }
  literal.Complete();
  ASSERT_NOT_NULL(next_.literal_chars);
  number_ = StringToDouble(next_.literal_chars->ascii_literal(),
                           NO_FLAGS,  // Hex, octal or trailing junk.
                           OS::nan_value());
  return Token::NUMBER;
}


Token::Value JsonScanner::ScanJsonIdentifier(const char* text,
                                             Token::Value token) {
  LiteralScope literal(this);
  while (*text != '\0') {
    if (c0_ != *text) return Token::ILLEGAL;
    Advance();
    text++;
  }
  if (ScannerConstants::kIsIdentifierPart.get(c0_)) return Token::ILLEGAL;
  literal.Complete();
  return token;
}



} }  // namespace v8::internal
