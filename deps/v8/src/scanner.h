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
    ASSERT_NOT_NULL(data_);
    if (cursor_ <= limit_ &&
        static_cast<unsigned>(c) <= unibrow::Utf8::kMaxOneByteChar) {
      *cursor_++ = static_cast<char>(c);
    } else {
      AddCharSlow(c);
    }
  }

  void Reset() {
    if (data_ == NULL) {
      data_ = NewArray<char>(kInitialCapacity);
      limit_ = ComputeLimit(data_, kInitialCapacity);
    }
    cursor_ = data_;
  }

  int pos() const {
    ASSERT_NOT_NULL(data_);
    return static_cast<int>(cursor_ - data_);
  }

  char* data() const { return data_; }

 private:
  static const int kInitialCapacity = 256;
  char* data_;
  char* cursor_;
  char* limit_;

  int Capacity() const {
    ASSERT_NOT_NULL(data_);
    return static_cast<int>(limit_ - data_) + unibrow::Utf8::kMaxEncodedSize;
  }

  static char* ComputeLimit(char* data, int capacity) {
    return (data + capacity) - unibrow::Utf8::kMaxEncodedSize;
  }

  void AddCharSlow(uc32 c);
};


// Interface through which the scanner reads characters from the input source.
class UTF16Buffer {
 public:
  UTF16Buffer();
  virtual ~UTF16Buffer() {}

  virtual void PushBack(uc32 ch) = 0;
  // Returns a value < 0 when the buffer end is reached.
  virtual uc32 Advance() = 0;
  virtual void SeekForward(int pos) = 0;

  int pos() const { return pos_; }

 protected:
  int pos_;  // Current position in the buffer.
  int end_;  // Position where scanning should stop (EOF).
};


// UTF16 buffer to read characters from a character stream.
class CharacterStreamUTF16Buffer: public UTF16Buffer {
 public:
  CharacterStreamUTF16Buffer();
  virtual ~CharacterStreamUTF16Buffer() {}
  void Initialize(Handle<String> data,
                  unibrow::CharacterStream* stream,
                  int start_position,
                  int end_position);
  virtual void PushBack(uc32 ch);
  virtual uc32 Advance();
  virtual void SeekForward(int pos);

 private:
  List<uc32> pushback_buffer_;
  uc32 last_;
  unibrow::CharacterStream* stream_;

  List<uc32>* pushback_buffer() { return &pushback_buffer_; }
};


// UTF16 buffer to read characters from an external string.
template <typename StringType, typename CharType>
class ExternalStringUTF16Buffer: public UTF16Buffer {
 public:
  ExternalStringUTF16Buffer();
  virtual ~ExternalStringUTF16Buffer() {}
  void Initialize(Handle<StringType> data,
                  int start_position,
                  int end_position);
  virtual void PushBack(uc32 ch);
  virtual uc32 Advance();
  virtual void SeekForward(int pos);

 private:
  const CharType* raw_data_;  // Pointer to the actual array of characters.
};


class KeywordMatcher {
//  Incrementally recognize keywords.
//
//  Recognized keywords:
//      break case catch const* continue debugger* default delete do else
//      finally false for function if in instanceof native* new null
//      return switch this throw true try typeof var void while with
//
//  *: Actually "future reserved keywords". These are the only ones we
//     recognized, the remaining are allowed as identifiers.
 public:
  KeywordMatcher() : state_(INITIAL), token_(Token::IDENTIFIER) {}

  Token::Value token() { return token_; }

  inline void AddChar(uc32 input) {
    if (state_ != UNMATCHABLE) {
      Step(input);
    }
  }

  void Fail() {
    token_ = Token::IDENTIFIER;
    state_ = UNMATCHABLE;
  }

 private:
  enum State {
    UNMATCHABLE,
    INITIAL,
    KEYWORD_PREFIX,
    KEYWORD_MATCHED,
    C,
    CA,
    CO,
    CON,
    D,
    DE,
    F,
    I,
    IN,
    N,
    T,
    TH,
    TR,
    V,
    W
  };

  struct FirstState {
    const char* keyword;
    State state;
    Token::Value token;
  };

  // Range of possible first characters of a keyword.
  static const unsigned int kFirstCharRangeMin = 'b';
  static const unsigned int kFirstCharRangeMax = 'w';
  static const unsigned int kFirstCharRangeLength =
      kFirstCharRangeMax - kFirstCharRangeMin + 1;
  // State map for first keyword character range.
  static FirstState first_states_[kFirstCharRangeLength];

  // Current state.
  State state_;
  // Token for currently added characters.
  Token::Value token_;

  // Matching a specific keyword string (there is only one possible valid
  // keyword with the current prefix).
  const char* keyword_;
  int counter_;
  Token::Value keyword_token_;

  // If input equals keyword's character at position, continue matching keyword
  // from that position.
  inline bool MatchKeywordStart(uc32 input,
                                const char* keyword,
                                int position,
                                Token::Value token_if_match) {
    if (input == keyword[position]) {
      state_ = KEYWORD_PREFIX;
      this->keyword_ = keyword;
      this->counter_ = position + 1;
      this->keyword_token_ = token_if_match;
      return true;
    }
    return false;
  }

  // If input equals match character, transition to new state and return true.
  inline bool MatchState(uc32 input, char match, State new_state) {
    if (input == match) {
      state_ = new_state;
      return true;
    }
    return false;
  }

  inline bool MatchKeyword(uc32 input,
                           char match,
                           State new_state,
                           Token::Value keyword_token) {
    if (input == match) {  // Matched "do".
      state_ = new_state;
      token_ = keyword_token;
      return true;
    }
    return false;
  }

  void Step(uc32 input);
};


enum ParserMode { PARSE, PREPARSE };
enum ParserLanguage { JAVASCRIPT, JSON };


class Scanner {
 public:
  typedef unibrow::Utf8InputBuffer<1024> Utf8Decoder;

  // Construction
  explicit Scanner(ParserMode parse_mode);

  // Initialize the Scanner to scan source.
  void Initialize(Handle<String> source,
                  ParserLanguage language);
  void Initialize(Handle<String> source,
                  unibrow::CharacterStream* stream,
                  ParserLanguage language);
  void Initialize(Handle<String> source,
                  int start_position, int end_position,
                  ParserLanguage language);

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
  // These functions only give the correct result if the literal
  // was scanned between calls to StartLiteral() and TerminateLiteral().
  const char* literal_string() const {
    return current_.literal_buffer->data();
  }
  int literal_length() const {
    // Excluding terminal '\0' added by TerminateLiteral().
    return current_.literal_buffer->pos() - 1;
  }

  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  const char* next_literal_string() const {
    return next_.literal_buffer->data();
  }
  // Returns the length of the next token (that would be returned if
  // Next() were called).
  int next_literal_length() const {
    return next_.literal_buffer->pos() - 1;
  }

  Vector<const char> next_literal() const {
    return Vector<const char>(next_literal_string(),
                              next_literal_length());
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

  static StaticResource<Utf8Decoder>* utf8_decoder() { return &utf8_decoder_; }

  // Tells whether the buffer contains an identifier (no escapes).
  // Used for checking if a property name is an identifier.
  static bool IsIdentifier(unibrow::CharacterStream* buffer);

  static unibrow::Predicate<IdentifierStart, 128> kIsIdentifierStart;
  static unibrow::Predicate<IdentifierPart, 128> kIsIdentifierPart;
  static unibrow::Predicate<unibrow::LineTerminator, 128> kIsLineTerminator;
  static unibrow::Predicate<unibrow::WhiteSpace, 128> kIsWhiteSpace;

  static const int kCharacterLookaheadBufferSize = 1;
  static const int kNoEndPosition = 1;

 private:
  void Init(Handle<String> source,
            unibrow::CharacterStream* stream,
            int start_position, int end_position,
            ParserLanguage language);


  // Different UTF16 buffers used to pull characters from. Based on input one of
  // these will be initialized as the actual data source.
  CharacterStreamUTF16Buffer char_stream_buffer_;
  ExternalStringUTF16Buffer<ExternalTwoByteString, uint16_t>
      two_byte_string_buffer_;
  ExternalStringUTF16Buffer<ExternalAsciiString, char> ascii_string_buffer_;

  // Source. Will point to one of the buffers declared above.
  UTF16Buffer* source_;

  // Used to convert the source string into a character stream when a stream
  // is not passed to the scanner.
  SafeStringInputBuffer safe_string_input_buffer_;

  // Buffer to hold literal values (identifiers, strings, numbers)
  // using 0-terminated UTF-8 encoding.
  UTF8Buffer literal_buffer_1_;
  UTF8Buffer literal_buffer_2_;

  bool stack_overflow_;
  static StaticResource<Utf8Decoder> utf8_decoder_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;

  // The current and look-ahead token.
  struct TokenDesc {
    Token::Value token;
    Location location;
    UTF8Buffer* literal_buffer;
  };

  TokenDesc current_;  // desc for current token (as returned by Next())
  TokenDesc next_;     // desc for next token (one token look-ahead)
  bool has_line_terminator_before_next_;
  bool is_pre_parsing_;
  bool is_parsing_json_;

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

  bool SkipWhiteSpace() {
    if (is_parsing_json_) {
      return SkipJsonWhiteSpace();
    } else {
      return SkipJavaScriptWhiteSpace();
    }
  }
  bool SkipJavaScriptWhiteSpace();
  bool SkipJsonWhiteSpace();
  Token::Value SkipSingleLineComment();
  Token::Value SkipMultiLineComment();

  inline Token::Value Select(Token::Value tok);
  inline Token::Value Select(uc32 next, Token::Value then, Token::Value else_);

  inline void Scan() {
    if (is_parsing_json_) {
      ScanJson();
    } else {
      ScanJavaScript();
    }
  }

  // Scans a single JavaScript token.
  void ScanJavaScript();

  // Scan a single JSON token. The JSON lexical grammar is specified in the
  // ECMAScript 5 standard, section 15.12.1.1.
  // Recognizes all of the single-character tokens directly, or calls a function
  // to scan a number, string or identifier literal.
  // The only allowed whitespace characters between tokens are tab,
  // carrige-return, newline and space.
  void ScanJson();

  // A JSON number (production JSONNumber) is a subset of the valid JavaScript
  // decimal number literals.
  // It includes an optional minus sign, must have at least one
  // digit before and after a decimal point, may not have prefixed zeros (unless
  // the integer part is zero), and may include an exponent part (e.g., "e-10").
  // Hexadecimal and octal numbers are not allowed.
  Token::Value ScanJsonNumber();
  // A JSON string (production JSONString) is subset of valid JavaScript string
  // literals. The string must only be double-quoted (not single-quoted), and
  // the only allowed backslash-escapes are ", /, \, b, f, n, r, t and
  // four-digit hex escapes (uXXXX). Any other use of backslashes is invalid.
  Token::Value ScanJsonString();
  // Used to recognizes one of the literals "true", "false", or "null". These
  // are the only valid JSON identifiers (productions JSONBooleanLiteral,
  // JSONNullLiteral).
  Token::Value ScanJsonIdentifier(const char* text, Token::Value token);

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
    return source_->pos() - kCharacterLookaheadBufferSize;
  }

  // Decodes a unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadRune.
  uc32 ScanIdentifierUnicodeEscape();
};

} }  // namespace v8::internal

#endif  // V8_SCANNER_H_
