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

#ifndef V8_SCANNER_H_
#define V8_SCANNER_H_

#include "token.h"
#include "char-predicates-inl.h"

namespace v8 {
namespace internal {


class UTF8Buffer {
 public:
  UTF8Buffer();
  ~UTF8Buffer();

  void AddChar(uc32 c) {
    if (cursor_ <= limit_ &&
        static_cast<unsigned>(c) <= unibrow::Utf8::kMaxOneByteChar) {
      *cursor_++ = static_cast<char>(c);
    } else {
      AddCharSlow(c);
    }
  }

  void Reset() { cursor_ = data_; }
  int pos() const { return cursor_ - data_; }
  char* data() const { return data_; }

 private:
  char* data_;
  char* cursor_;
  char* limit_;

  int Capacity() const {
    return (limit_ - data_) + unibrow::Utf8::kMaxEncodedSize;
  }

  static char* ComputeLimit(char* data, int capacity) {
    return (data + capacity) - unibrow::Utf8::kMaxEncodedSize;
  }

  void AddCharSlow(uc32 c);
};


class UTF16Buffer {
 public:
  UTF16Buffer();
  virtual ~UTF16Buffer() {}

  virtual void PushBack(uc32 ch) = 0;
  // returns a value < 0 when the buffer end is reached
  virtual uc32 Advance() = 0;
  virtual void SeekForward(int pos) = 0;

  int pos() const { return pos_; }
  int size() const { return size_; }
  Handle<String> SubString(int start, int end);

 protected:
  Handle<String> data_;
  int pos_;
  int size_;
};


class CharacterStreamUTF16Buffer: public UTF16Buffer {
 public:
  CharacterStreamUTF16Buffer();
  virtual ~CharacterStreamUTF16Buffer() {}
  void Initialize(Handle<String> data, unibrow::CharacterStream* stream);
  virtual void PushBack(uc32 ch);
  virtual uc32 Advance();
  virtual void SeekForward(int pos);

 private:
  List<uc32> pushback_buffer_;
  uc32 last_;
  unibrow::CharacterStream* stream_;

  List<uc32>* pushback_buffer() { return &pushback_buffer_; }
};


class TwoByteStringUTF16Buffer: public UTF16Buffer {
 public:
  TwoByteStringUTF16Buffer();
  virtual ~TwoByteStringUTF16Buffer() {}
  void Initialize(Handle<ExternalTwoByteString> data);
  virtual void PushBack(uc32 ch);
  virtual uc32 Advance();
  virtual void SeekForward(int pos);

 private:
  const uint16_t* raw_data_;
};


class Scanner {
 public:

  typedef unibrow::Utf8InputBuffer<1024> Utf8Decoder;

  // Construction
  explicit Scanner(bool is_pre_parsing);

  // Initialize the Scanner to scan source:
  void Init(Handle<String> source,
            unibrow::CharacterStream* stream,
            int position);

  // Returns the next token.
  Token::Value Next();

  // One token look-ahead (past the token returned by Next()).
  Token::Value peek() const  { return next_.token; }

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
  Location location() const  { return current_.location; }
  Location peek_location() const  { return next_.location; }

  // Returns the literal string, if any, for the current token (the
  // token returned by Next()). The string is 0-terminated and in
  // UTF-8 format; they may contain 0-characters. Literal strings are
  // collected for identifiers, strings, and numbers.
  const char* literal_string() const {
    return &literals_.data()[current_.literal_pos];
  }
  int literal_length() const {
    return current_.literal_end - current_.literal_pos;
  }

  Vector<const char> next_literal() const {
    return Vector<const char>(next_literal_string(), next_literal_length());
  }

  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  const char* next_literal_string() const {
    return &literals_.data()[next_.literal_pos];
  }
  // Returns the length of the next token (that would be returned if
  // Next() were called).
  int next_literal_length() const {
    return next_.literal_end - next_.literal_pos;
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

  Handle<String> SubString(int start_pos, int end_pos);
  bool stack_overflow() { return stack_overflow_; }

  static StaticResource<Utf8Decoder>* utf8_decoder() { return &utf8_decoder_; }

  // Tells whether the buffer contains an identifier (no escapes).
  // Used for checking if a property name is an identifier.
  static bool IsIdentifier(unibrow::CharacterStream* buffer);

  static unibrow::Predicate<IdentifierStart, 128> kIsIdentifierStart;
  static unibrow::Predicate<IdentifierPart, 128> kIsIdentifierPart;
  static unibrow::Predicate<unibrow::LineTerminator, 128> kIsLineTerminator;
  static unibrow::Predicate<unibrow::WhiteSpace, 128> kIsWhiteSpace;

 private:
  CharacterStreamUTF16Buffer char_stream_buffer_;
  TwoByteStringUTF16Buffer two_byte_string_buffer_;

  // Source.
  UTF16Buffer* source_;
  int position_;

  // Buffer to hold literal values (identifiers, strings, numbers)
  // using 0-terminated UTF-8 encoding.
  UTF8Buffer literals_;

  bool stack_overflow_;
  static StaticResource<Utf8Decoder> utf8_decoder_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;

  // The current and look-ahead token.
  struct TokenDesc {
    Token::Value token;
    Location location;
    int literal_pos, literal_end;
  };

  TokenDesc current_;  // desc for current token (as returned by Next())
  TokenDesc next_;     // desc for next token (one token look-ahead)
  bool has_line_terminator_before_next_;
  bool is_pre_parsing_;

  static const int kCharacterLookaheadBufferSize = 1;

  // Literal buffer support
  void StartLiteral();
  void AddChar(uc32 ch);
  void AddCharAdvance();
  void TerminateLiteral();

  // Low-level scanning support.
  void Advance() { c0_ = source_->Advance(); }
  void PushBack(uc32 ch) {
    source_->PushBack(ch);
    c0_ = ch;
  }

  bool SkipWhiteSpace();
  Token::Value SkipSingleLineComment();
  Token::Value SkipMultiLineComment();

  inline Token::Value Select(Token::Value tok);
  inline Token::Value Select(uc32 next, Token::Value then, Token::Value else_);

  void Scan();
  void ScanDecimalDigits();
  Token::Value ScanNumber(bool seen_period);
  Token::Value ScanIdentifier();
  uc32 ScanHexEscape(uc32 c, int length);
  uc32 ScanOctalEscape(uc32 c, int length);
  void ScanEscape();
  Token::Value ScanString();

  // Scans a possible HTML comment -- begins with '<!'.
  Token::Value ScanHtmlComment();

  // Return the current source position.
  int source_pos() {
    return source_->pos() - kCharacterLookaheadBufferSize + position_;
  }

  // Decodes a unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadRune.
  uc32 ScanIdentifierUnicodeEscape();
};

} }  // namespace v8::internal

#endif  // V8_SCANNER_H_
