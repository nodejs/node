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
// UTF16Buffer

// CharacterStreamUTF16Buffer
CharacterStreamUTF16Buffer::CharacterStreamUTF16Buffer()
    : pushback_buffer_(0), last_(0), stream_(NULL) { }


void CharacterStreamUTF16Buffer::Initialize(Handle<String> data,
                                            unibrow::CharacterStream* input,
                                            int start_position,
                                            int end_position) {
  stream_ = input;
  if (start_position > 0) {
    SeekForward(start_position);
  }
  end_ = end_position != kNoEndPosition ? end_position : kMaxInt;
}


void CharacterStreamUTF16Buffer::PushBack(uc32 ch) {
  pushback_buffer()->Add(last_);
  last_ = ch;
  pos_--;
}


uc32 CharacterStreamUTF16Buffer::Advance() {
  ASSERT(end_ != kNoEndPosition);
  ASSERT(end_ >= 0);
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
  } else if (stream_->has_more() && pos_ < end_) {
    pos_++;
    uc32 next = stream_->GetNext();
    return last_ = next;
  } else {
    // Note: currently the following increment is necessary to avoid a
    // test-parser problem!
    pos_++;
    return last_ = static_cast<uc32>(-1);
  }
}


void CharacterStreamUTF16Buffer::SeekForward(int pos) {
  pos_ = pos;
  ASSERT(pushback_buffer()->is_empty());
  stream_->Seek(pos);
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

void V8JavaScriptScanner::Initialize(Handle<String> source,
                                     int literal_flags) {
  source_ = stream_initializer_.Init(source, NULL, 0, source->length());
  // Need to capture identifiers in order to recognize "get" and "set"
  // in object literals.
  literal_flags_ = literal_flags | kLiteralIdentifier;
  Init();
  // Skip initial whitespace allowing HTML comment ends just like
  // after a newline and scan first token.
  has_line_terminator_before_next_ = true;
  SkipWhiteSpace();
  Scan();
}


void V8JavaScriptScanner::Initialize(Handle<String> source,
                                     unibrow::CharacterStream* stream,
                                     int literal_flags) {
  source_ = stream_initializer_.Init(source, stream,
                                     0, UTF16Buffer::kNoEndPosition);
  literal_flags_ = literal_flags | kLiteralIdentifier;
  Init();
  // Skip initial whitespace allowing HTML comment ends just like
  // after a newline and scan first token.
  has_line_terminator_before_next_ = true;
  SkipWhiteSpace();
  Scan();
}


void V8JavaScriptScanner::Initialize(Handle<String> source,
                                     int start_position,
                                     int end_position,
                                     int literal_flags) {
  source_ = stream_initializer_.Init(source, NULL,
                                     start_position, end_position);
  literal_flags_ = literal_flags | kLiteralIdentifier;
  Init();
  // Skip initial whitespace allowing HTML comment ends just like
  // after a newline and scan first token.
  has_line_terminator_before_next_ = true;
  SkipWhiteSpace();
  Scan();
}


UTF16Buffer* StreamInitializer::Init(Handle<String> source,
                                     unibrow::CharacterStream* stream,
                                     int start_position,
                                     int end_position) {
  // Either initialize the scanner from a character stream or from a
  // string.
  ASSERT(source.is_null() || stream == NULL);

  // Initialize the source buffer.
  if (!source.is_null() && StringShape(*source).IsExternalTwoByte()) {
    two_byte_string_buffer_.Initialize(
        Handle<ExternalTwoByteString>::cast(source),
        start_position,
        end_position);
    return &two_byte_string_buffer_;
  } else if (!source.is_null() && StringShape(*source).IsExternalAscii()) {
    ascii_string_buffer_.Initialize(
        Handle<ExternalAsciiString>::cast(source),
        start_position,
        end_position);
    return &ascii_string_buffer_;
  } else {
    if (!source.is_null()) {
      safe_string_input_buffer_.Reset(source.location());
      stream = &safe_string_input_buffer_;
    }
    char_stream_buffer_.Initialize(source,
                                   stream,
                                   start_position,
                                   end_position);
    return &char_stream_buffer_;
  }
}

// ----------------------------------------------------------------------------
// JsonScanner

JsonScanner::JsonScanner() {}


void JsonScanner::Initialize(Handle<String> source) {
  source_ = stream_initializer_.Init(source, NULL, 0, source->length());
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
  next_.literal_chars = Vector<const char>();
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
  while (c0_ != '"' && c0_ > 0) {
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
  if (c0_ != '"') {
    return Token::ILLEGAL;
  }
  literal.Complete();
  Advance();
  return Token::STRING;
}


Token::Value JsonScanner::ScanJsonNumber() {
  LiteralScope literal(this);
  if (c0_ == '-') AddLiteralCharAdvance();
  if (c0_ == '0') {
    AddLiteralCharAdvance();
    // Prefix zero is only allowed if it's the only digit before
    // a decimal point or exponent.
    if ('0' <= c0_ && c0_ <= '9') return Token::ILLEGAL;
  } else {
    if (c0_ < '1' || c0_ > '9') return Token::ILLEGAL;
    do {
      AddLiteralCharAdvance();
    } while (c0_ >= '0' && c0_ <= '9');
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
