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
  inline int32_t Advance() {
    if (buffer_cursor_ < buffer_end_ || ReadBlock()) {
      pos_++;
      return *(buffer_cursor_++);
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

  // Pushes back the most recently read UC16 character, i.e.,
  // the value returned by the most recent call to Advance.
  // Must not be used right after calling SeekForward.
  virtual void PushBack(uc16 character) = 0;

 protected:
  static const int32_t kEndOfInput = -1;

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
// LiteralCollector -  Collector of chars of literals.

class LiteralCollector {
 public:
  LiteralCollector();
  ~LiteralCollector();

  inline void AddChar(uc32 c) {
    if (recording_) {
      if (static_cast<unsigned>(c) <= unibrow::Utf8::kMaxOneByteChar) {
        buffer_.Add(static_cast<char>(c));
      } else {
        AddCharSlow(c);
      }
    }
  }

  void StartLiteral() {
    buffer_.StartSequence();
    recording_ = true;
  }

  Vector<const char> EndLiteral() {
    if (recording_) {
      recording_ = false;
      buffer_.Add(kEndMarker);
      Vector<char> sequence = buffer_.EndSequence();
      return Vector<const char>(sequence.start(), sequence.length());
    }
    return Vector<const char>();
  }

  void DropLiteral() {
    if (recording_) {
      recording_ = false;
      buffer_.DropSequence();
    }
  }

  void Reset() {
    buffer_.Reset();
  }

  // The end marker added after a parsed literal.
  // Using zero allows the usage of strlen and similar functions on
  // identifiers and numbers (but not strings, since they may contain zero
  // bytes).
  static const char kEndMarker = '\x00';
 private:
  static const int kInitialCapacity = 256;
  SequenceCollector<char, 4> buffer_;
  bool recording_;
  void AddCharSlow(uc32 c);
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
  const char* literal_string() const {
    return current_.literal_chars.start();
  }

  int literal_length() const {
    // Excluding terminal '\x00' added by TerminateLiteral().
    return current_.literal_chars.length() - 1;
  }

  Vector<const char> literal() const {
    return Vector<const char>(literal_string(), literal_length());
  }

  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  const char* next_literal_string() const {
    return next_.literal_chars.start();
  }


  // Returns the length of the next token (that would be returned if
  // Next() were called).
  int next_literal_length() const {
    // Excluding terminal '\x00' added by TerminateLiteral().
    return next_.literal_chars.length() - 1;
  }

  Vector<const char> next_literal() const {
    return Vector<const char>(next_literal_string(), next_literal_length());
  }

  static const int kCharacterLookaheadBufferSize = 1;

 protected:
  // The current and look-ahead token.
  struct TokenDesc {
    Token::Value token;
    Location location;
    Vector<const char> literal_chars;
  };

  // Call this after setting source_ to the input.
  void Init() {
    // Set c0_ (one character ahead)
    ASSERT(kCharacterLookaheadBufferSize == 1);
    Advance();
    // Initialize current_ to not refer to a literal.
    current_.literal_chars = Vector<const char>();
    // Reset literal buffer.
    literal_buffer_.Reset();
  }

  // Literal buffer support
  inline void StartLiteral() {
    literal_buffer_.StartLiteral();
  }

  inline void AddLiteralChar(uc32 c) {
    literal_buffer_.AddChar(c);
  }

  // Complete scanning of a literal.
  inline void TerminateLiteral() {
    next_.literal_chars = literal_buffer_.EndLiteral();
  }

  // Stops scanning of a literal and drop the collected characters,
  // e.g., due to an encountered error.
  inline void DropLiteral() {
    literal_buffer_.DropLiteral();
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

  TokenDesc current_;  // desc for current token (as returned by Next())
  TokenDesc next_;     // desc for next token (one token look-ahead)

  // Input stream. Must be initialized to an UC16CharacterStream.
  UC16CharacterStream* source_;

  // Buffer to hold literal values (identifiers, strings, numbers)
  // using '\x00'-terminated UTF-8 encoding. Handles allocation internally.
  LiteralCollector literal_buffer_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;
};

// ----------------------------------------------------------------------------
// JavaScriptScanner - base logic for JavaScript scanning.

class JavaScriptScanner : public Scanner {
 public:

  // Bit vector representing set of types of literals.
  enum LiteralType {
    kNoLiterals = 0,
    kLiteralNumber = 1,
    kLiteralIdentifier = 2,
    kLiteralString = 4,
    kLiteralRegExp = 8,
    kLiteralRegExpFlags = 16,
    kAllLiterals = 31
  };

  // A LiteralScope that disables recording of some types of JavaScript
  // literals. If the scanner is configured to not record the specific
  // type of literal, the scope will not call StartLiteral.
  class LiteralScope {
   public:
    LiteralScope(JavaScriptScanner* self, LiteralType type)
        : scanner_(self), complete_(false) {
      if (scanner_->RecordsLiteral(type)) {
        scanner_->StartLiteral();
      }
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

  // Whether this scanner records the given literal type or not.
  bool RecordsLiteral(LiteralType type) {
    return (literal_flags_ & type) != 0;
  }

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

  int literal_flags_;
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
