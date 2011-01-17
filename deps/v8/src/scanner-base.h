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

// Features shared by parsing and pre-parsing scanners.

#ifndef V8_SCANNER_BASE_H_
#define V8_SCANNER_BASE_H_

#include "globals.h"
#include "checks.h"
#include "allocation.h"
#include "token.h"
#include "unicode-inl.h"
#include "char-predicates.h"
#include "utils.h"
#include "list-inl.h"

namespace v8 {
namespace internal {

// Returns the value (0 .. 15) of a hexadecimal character c.
// If c is not a legal hexadecimal character, returns a value < 0.
inline int HexValue(uc32 c) {
  c -= '0';
  if (static_cast<unsigned>(c) <= 9) return c;
  c = (c | 0x20) - ('a' - '0');  // detect 0x11..0x16 and 0x31..0x36.
  if (static_cast<unsigned>(c) <= 5) return c + 10;
  return -1;
}


// ---------------------------------------------------------------------
// Buffered stream of characters, using an internal UC16 buffer.

class UC16CharacterStream {
 public:
  UC16CharacterStream() : pos_(0) { }
  virtual ~UC16CharacterStream() { }

  // Returns and advances past the next UC16 character in the input
  // stream. If there are no more characters, it returns a negative
  // value.
  inline uc32 Advance() {
    if (buffer_cursor_ < buffer_end_ || ReadBlock()) {
      pos_++;
      return static_cast<uc32>(*(buffer_cursor_++));
    }
    // Note: currently the following increment is necessary to avoid a
    // parser problem! The scanner treats the final kEndOfInput as
    // a character with a position, and does math relative to that
    // position.
    pos_++;

    return kEndOfInput;
  }

  // Return the current position in the character stream.
  // Starts at zero.
  inline unsigned pos() const { return pos_; }

  // Skips forward past the next character_count UC16 characters
  // in the input, or until the end of input if that comes sooner.
  // Returns the number of characters actually skipped. If less
  // than character_count,
  inline unsigned SeekForward(unsigned character_count) {
    unsigned buffered_chars =
        static_cast<unsigned>(buffer_end_ - buffer_cursor_);
    if (character_count <= buffered_chars) {
      buffer_cursor_ += character_count;
      pos_ += character_count;
      return character_count;
    }
    return SlowSeekForward(character_count);
  }

  // Pushes back the most recently read UC16 character (or negative
  // value if at end of input), i.e., the value returned by the most recent
  // call to Advance.
  // Must not be used right after calling SeekForward.
  virtual void PushBack(int32_t character) = 0;

 protected:
  static const uc32 kEndOfInput = -1;

  // Ensures that the buffer_cursor_ points to the character at
  // position pos_ of the input, if possible. If the position
  // is at or after the end of the input, return false. If there
  // are more characters available, return true.
  virtual bool ReadBlock() = 0;
  virtual unsigned SlowSeekForward(unsigned character_count) = 0;

  const uc16* buffer_cursor_;
  const uc16* buffer_end_;
  unsigned pos_;
};


// ---------------------------------------------------------------------
// Constants used by scanners.

class ScannerConstants : AllStatic {
 public:
  typedef unibrow::Utf8InputBuffer<1024> Utf8Decoder;

  static StaticResource<Utf8Decoder>* utf8_decoder() {
    return &utf8_decoder_;
  }

  static unibrow::Predicate<IdentifierStart, 128> kIsIdentifierStart;
  static unibrow::Predicate<IdentifierPart, 128> kIsIdentifierPart;
  static unibrow::Predicate<unibrow::LineTerminator, 128> kIsLineTerminator;
  static unibrow::Predicate<unibrow::WhiteSpace, 128> kIsWhiteSpace;

  static bool IsIdentifier(unibrow::CharacterStream* buffer);

 private:
  static StaticResource<Utf8Decoder> utf8_decoder_;
};

// ----------------------------------------------------------------------------
// LiteralBuffer -  Collector of chars of literals.

class LiteralBuffer {
 public:
  LiteralBuffer() : is_ascii_(true), position_(0), backing_store_() { }

  ~LiteralBuffer() {
    if (backing_store_.length() > 0) {
      backing_store_.Dispose();
    }
  }

  inline void AddChar(uc16 character) {
    if (position_ >= backing_store_.length()) ExpandBuffer();
    if (is_ascii_) {
      if (character < kMaxAsciiCharCodeU) {
        backing_store_[position_] = static_cast<byte>(character);
        position_ += kASCIISize;
        return;
      }
      ConvertToUC16();
    }
    *reinterpret_cast<uc16*>(&backing_store_[position_]) = character;
    position_ += kUC16Size;
  }

  bool is_ascii() { return is_ascii_; }

  Vector<const uc16> uc16_literal() {
    ASSERT(!is_ascii_);
    ASSERT((position_ & 0x1) == 0);
    return Vector<const uc16>(
        reinterpret_cast<const uc16*>(backing_store_.start()),
        position_ >> 1);
  }

  Vector<const char> ascii_literal() {
    ASSERT(is_ascii_);
    return Vector<const char>(
        reinterpret_cast<const char*>(backing_store_.start()),
        position_);
  }

  int length() {
    return is_ascii_ ? position_ : (position_ >> 1);
  }

  void Reset() {
    position_ = 0;
    is_ascii_ = true;
  }
 private:
  static const int kInitialCapacity = 16;
  static const int kGrowthFactory = 4;
  static const int kMinConversionSlack = 256;
  static const int kMaxGrowth = 1 * MB;
  inline int NewCapacity(int min_capacity) {
    int capacity = Max(min_capacity, backing_store_.length());
    int new_capacity = Min(capacity * kGrowthFactory, capacity + kMaxGrowth);
    return new_capacity;
  }

  void ExpandBuffer() {
    Vector<byte> new_store = Vector<byte>::New(NewCapacity(kInitialCapacity));
    memcpy(new_store.start(), backing_store_.start(), position_);
    backing_store_.Dispose();
    backing_store_ = new_store;
  }

  void ConvertToUC16() {
    ASSERT(is_ascii_);
    Vector<byte> new_store;
    int new_content_size = position_ * kUC16Size;
    if (new_content_size >= backing_store_.length()) {
      // Ensure room for all currently read characters as UC16 as well
      // as the character about to be stored.
      new_store = Vector<byte>::New(NewCapacity(new_content_size));
    } else {
      new_store = backing_store_;
    }
    char* src = reinterpret_cast<char*>(backing_store_.start());
    uc16* dst = reinterpret_cast<uc16*>(new_store.start());
    for (int i = position_ - 1; i >= 0; i--) {
      dst[i] = src[i];
    }
    if (new_store.start() != backing_store_.start()) {
      backing_store_.Dispose();
      backing_store_ = new_store;
    }
    position_ = new_content_size;
    is_ascii_ = false;
  }

  bool is_ascii_;
  int position_;
  Vector<byte> backing_store_;
};


// ----------------------------------------------------------------------------
// Scanner base-class.

// Generic functionality used by both JSON and JavaScript scanners.
class Scanner {
 public:
  typedef unibrow::Utf8InputBuffer<1024> Utf8Decoder;

  class LiteralScope {
   public:
    explicit LiteralScope(Scanner* self);
    ~LiteralScope();
    void Complete();

   private:
    Scanner* scanner_;
    bool complete_;
  };

  Scanner();

  // Returns the current token again.
  Token::Value current_token() { return current_.token; }

  // One token look-ahead (past the token returned by Next()).
  Token::Value peek() const { return next_.token; }

  struct Location {
    Location(int b, int e) : beg_pos(b), end_pos(e) { }
    Location() : beg_pos(0), end_pos(0) { }
    int beg_pos;
    int end_pos;
  };

  // Returns the location information for the current token
  // (the token returned by Next()).
  Location location() const { return current_.location; }
  Location peek_location() const { return next_.location; }

  // Returns the literal string, if any, for the current token (the
  // token returned by Next()). The string is 0-terminated and in
  // UTF-8 format; they may contain 0-characters. Literal strings are
  // collected for identifiers, strings, and numbers.
  // These functions only give the correct result if the literal
  // was scanned between calls to StartLiteral() and TerminateLiteral().
  bool is_literal_ascii() {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->is_ascii();
  }
  Vector<const char> literal_ascii_string() {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->ascii_literal();
  }
  Vector<const uc16> literal_uc16_string() {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->uc16_literal();
  }
  int literal_length() const {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->length();
  }

  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  bool is_next_literal_ascii() {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->is_ascii();
  }
  Vector<const char> next_literal_ascii_string() {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->ascii_literal();
  }
  Vector<const uc16> next_literal_uc16_string() {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->uc16_literal();
  }
  int next_literal_length() const {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->length();
  }

  static const int kCharacterLookaheadBufferSize = 1;

 protected:
  // The current and look-ahead token.
  struct TokenDesc {
    Token::Value token;
    Location location;
    LiteralBuffer* literal_chars;
  };

  // Call this after setting source_ to the input.
  void Init() {
    // Set c0_ (one character ahead)
    ASSERT(kCharacterLookaheadBufferSize == 1);
    Advance();
    // Initialize current_ to not refer to a literal.
    current_.literal_chars = NULL;
  }

  // Literal buffer support
  inline void StartLiteral() {
    LiteralBuffer* free_buffer = (current_.literal_chars == &literal_buffer1_) ?
            &literal_buffer2_ : &literal_buffer1_;
    free_buffer->Reset();
    next_.literal_chars = free_buffer;
  }

  inline void AddLiteralChar(uc32 c) {
    ASSERT_NOT_NULL(next_.literal_chars);
    next_.literal_chars->AddChar(c);
  }

  // Complete scanning of a literal.
  inline void TerminateLiteral() {
    // Does nothing in the current implementation.
  }

  // Stops scanning of a literal and drop the collected characters,
  // e.g., due to an encountered error.
  inline void DropLiteral() {
    next_.literal_chars = NULL;
  }

  inline void AddLiteralCharAdvance() {
    AddLiteralChar(c0_);
    Advance();
  }

  // Low-level scanning support.
  void Advance() { c0_ = source_->Advance(); }
  void PushBack(uc32 ch) {
    source_->PushBack(c0_);
    c0_ = ch;
  }

  inline Token::Value Select(Token::Value tok) {
    Advance();
    return tok;
  }

  inline Token::Value Select(uc32 next, Token::Value then, Token::Value else_) {
    Advance();
    if (c0_ == next) {
      Advance();
      return then;
    } else {
      return else_;
    }
  }

  uc32 ScanHexEscape(uc32 c, int length);
  uc32 ScanOctalEscape(uc32 c, int length);

  // Return the current source position.
  int source_pos() {
    return source_->pos() - kCharacterLookaheadBufferSize;
  }

  // Buffers collecting literal strings, numbers, etc.
  LiteralBuffer literal_buffer1_;
  LiteralBuffer literal_buffer2_;

  TokenDesc current_;  // desc for current token (as returned by Next())
  TokenDesc next_;     // desc for next token (one token look-ahead)

  // Input stream. Must be initialized to an UC16CharacterStream.
  UC16CharacterStream* source_;


  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;
};

// ----------------------------------------------------------------------------
// JavaScriptScanner - base logic for JavaScript scanning.

class JavaScriptScanner : public Scanner {
 public:
  // A LiteralScope that disables recording of some types of JavaScript
  // literals. If the scanner is configured to not record the specific
  // type of literal, the scope will not call StartLiteral.
  class LiteralScope {
   public:
    explicit LiteralScope(JavaScriptScanner* self)
        : scanner_(self), complete_(false) {
      scanner_->StartLiteral();
    }
     ~LiteralScope() {
       if (!complete_) scanner_->DropLiteral();
     }
    void Complete() {
      scanner_->TerminateLiteral();
      complete_ = true;
    }

   private:
    JavaScriptScanner* scanner_;
    bool complete_;
  };

  JavaScriptScanner();

  // Returns the next token.
  Token::Value Next();

  // Returns true if there was a line terminator before the peek'ed token.
  bool has_line_terminator_before_next() const {
    return has_line_terminator_before_next_;
  }

  // Scans the input as a regular expression pattern, previous
  // character(s) must be /(=). Returns true if a pattern is scanned.
  bool ScanRegExpPattern(bool seen_equal);
  // Returns true if regexp flags are scanned (always since flags can
  // be empty).
  bool ScanRegExpFlags();

  // Tells whether the buffer contains an identifier (no escapes).
  // Used for checking if a property name is an identifier.
  static bool IsIdentifier(unibrow::CharacterStream* buffer);

  // Seek forward to the given position.  This operation does not
  // work in general, for instance when there are pushed back
  // characters, but works for seeking forward until simple delimiter
  // tokens, which is what it is used for.
  void SeekForward(int pos);

 protected:
  bool SkipWhiteSpace();
  Token::Value SkipSingleLineComment();
  Token::Value SkipMultiLineComment();

  // Scans a single JavaScript token.
  void Scan();

  void ScanDecimalDigits();
  Token::Value ScanNumber(bool seen_period);
  Token::Value ScanIdentifierOrKeyword();
  Token::Value ScanIdentifierSuffix(LiteralScope* literal);

  void ScanEscape();
  Token::Value ScanString();

  // Scans a possible HTML comment -- begins with '<!'.
  Token::Value ScanHtmlComment();

  // Decodes a unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadChar.
  uc32 ScanIdentifierUnicodeEscape();

  bool has_line_terminator_before_next_;
};


// ----------------------------------------------------------------------------
// Keyword matching state machine.

class KeywordMatcher {
//  Incrementally recognize keywords.
//
//  Recognized keywords:
//      break case catch const* continue debugger* default delete do else
//      finally false for function if in instanceof native* new null
//      return switch this throw true try typeof var void while with
//
//  *: Actually "future reserved keywords". These are the only ones we
//     recognize, the remaining are allowed as identifiers.
//     In ES5 strict mode, we should disallow all reserved keywords.
 public:
  KeywordMatcher()
      : state_(INITIAL),
        token_(Token::IDENTIFIER),
        keyword_(NULL),
        counter_(0),
        keyword_token_(Token::ILLEGAL) {}

  Token::Value token() { return token_; }

  inline bool AddChar(unibrow::uchar input) {
    if (state_ != UNMATCHABLE) {
      Step(input);
    }
    return state_ != UNMATCHABLE;
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

  // If input equals keyword's character at position, continue matching keyword
  // from that position.
  inline bool MatchKeywordStart(unibrow::uchar input,
                                const char* keyword,
                                int position,
                                Token::Value token_if_match) {
    if (input != static_cast<unibrow::uchar>(keyword[position])) {
      return false;
    }
    state_ = KEYWORD_PREFIX;
    this->keyword_ = keyword;
    this->counter_ = position + 1;
    this->keyword_token_ = token_if_match;
    return true;
  }

  // If input equals match character, transition to new state and return true.
  inline bool MatchState(unibrow::uchar input, char match, State new_state) {
    if (input != static_cast<unibrow::uchar>(match)) {
      return false;
    }
    state_ = new_state;
    return true;
  }

  inline bool MatchKeyword(unibrow::uchar input,
                           char match,
                           State new_state,
                           Token::Value keyword_token) {
    if (input != static_cast<unibrow::uchar>(match)) {
      return false;
    }
    state_ = new_state;
    token_ = keyword_token;
    return true;
  }

  void Step(unibrow::uchar input);

  // Current state.
  State state_;
  // Token for currently added characters.
  Token::Value token_;

  // Matching a specific keyword string (there is only one possible valid
  // keyword with the current prefix).
  const char* keyword_;
  int counter_;
  Token::Value keyword_token_;
};


} }  // namespace v8::internal

#endif  // V8_SCANNER_BASE_H_
