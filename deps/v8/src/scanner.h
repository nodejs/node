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

#ifndef V8_SCANNER_H_
#define V8_SCANNER_H_

#include "allocation.h"
#include "char-predicates.h"
#include "checks.h"
#include "globals.h"
#include "token.h"
#include "unicode-inl.h"
#include "utils.h"

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
// Buffered stream of UTF-16 code units, using an internal UTF-16 buffer.
// A code unit is a 16 bit value representing either a 16 bit code point
// or one part of a surrogate pair that make a single 21 bit code point.

class Utf16CharacterStream {
 public:
  Utf16CharacterStream() : pos_(0) { }
  virtual ~Utf16CharacterStream() { }

  // Returns and advances past the next UTF-16 code unit in the input
  // stream. If there are no more code units, it returns a negative
  // value.
  inline uc32 Advance() {
    if (buffer_cursor_ < buffer_end_ || ReadBlock()) {
      pos_++;
      return static_cast<uc32>(*(buffer_cursor_++));
    }
    // Note: currently the following increment is necessary to avoid a
    // parser problem! The scanner treats the final kEndOfInput as
    // a code unit with a position, and does math relative to that
    // position.
    pos_++;

    return kEndOfInput;
  }

  // Return the current position in the code unit stream.
  // Starts at zero.
  inline unsigned pos() const { return pos_; }

  // Skips forward past the next code_unit_count UTF-16 code units
  // in the input, or until the end of input if that comes sooner.
  // Returns the number of code units actually skipped. If less
  // than code_unit_count,
  inline unsigned SeekForward(unsigned code_unit_count) {
    unsigned buffered_chars =
        static_cast<unsigned>(buffer_end_ - buffer_cursor_);
    if (code_unit_count <= buffered_chars) {
      buffer_cursor_ += code_unit_count;
      pos_ += code_unit_count;
      return code_unit_count;
    }
    return SlowSeekForward(code_unit_count);
  }

  // Pushes back the most recently read UTF-16 code unit (or negative
  // value if at end of input), i.e., the value returned by the most recent
  // call to Advance.
  // Must not be used right after calling SeekForward.
  virtual void PushBack(int32_t code_unit) = 0;

 protected:
  static const uc32 kEndOfInput = -1;

  // Ensures that the buffer_cursor_ points to the code_unit at
  // position pos_ of the input, if possible. If the position
  // is at or after the end of the input, return false. If there
  // are more code_units available, return true.
  virtual bool ReadBlock() = 0;
  virtual unsigned SlowSeekForward(unsigned code_unit_count) = 0;

  const uc16* buffer_cursor_;
  const uc16* buffer_end_;
  unsigned pos_;
};


class UnicodeCache {
// ---------------------------------------------------------------------
// Caching predicates used by scanners.
 public:
  UnicodeCache() {}
  typedef unibrow::Utf8Decoder<512> Utf8Decoder;

  StaticResource<Utf8Decoder>* utf8_decoder() {
    return &utf8_decoder_;
  }

  bool IsIdentifierStart(unibrow::uchar c) { return kIsIdentifierStart.get(c); }
  bool IsIdentifierPart(unibrow::uchar c) { return kIsIdentifierPart.get(c); }
  bool IsLineTerminator(unibrow::uchar c) { return kIsLineTerminator.get(c); }
  bool IsWhiteSpace(unibrow::uchar c) { return kIsWhiteSpace.get(c); }

 private:
  unibrow::Predicate<IdentifierStart, 128> kIsIdentifierStart;
  unibrow::Predicate<IdentifierPart, 128> kIsIdentifierPart;
  unibrow::Predicate<unibrow::LineTerminator, 128> kIsLineTerminator;
  unibrow::Predicate<unibrow::WhiteSpace, 128> kIsWhiteSpace;
  StaticResource<Utf8Decoder> utf8_decoder_;

  DISALLOW_COPY_AND_ASSIGN(UnicodeCache);
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

  INLINE(void AddChar(uint32_t code_unit)) {
    if (position_ >= backing_store_.length()) ExpandBuffer();
    if (is_ascii_) {
      if (code_unit <= unibrow::Latin1::kMaxChar) {
        backing_store_[position_] = static_cast<byte>(code_unit);
        position_ += kOneByteSize;
        return;
      }
      ConvertToUtf16();
    }
    ASSERT(code_unit < 0x10000u);
    *reinterpret_cast<uc16*>(&backing_store_[position_]) = code_unit;
    position_ += kUC16Size;
  }

  bool is_ascii() { return is_ascii_; }

  bool is_contextual_keyword(Vector<const char> keyword) {
    return is_ascii() && keyword.length() == position_ &&
        (memcmp(keyword.start(), backing_store_.start(), position_) == 0);
  }

  Vector<const uc16> utf16_literal() {
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
    OS::MemCopy(new_store.start(), backing_store_.start(), position_);
    backing_store_.Dispose();
    backing_store_ = new_store;
  }

  void ConvertToUtf16() {
    ASSERT(is_ascii_);
    Vector<byte> new_store;
    int new_content_size = position_ * kUC16Size;
    if (new_content_size >= backing_store_.length()) {
      // Ensure room for all currently read code units as UC16 as well
      // as the code unit about to be stored.
      new_store = Vector<byte>::New(NewCapacity(new_content_size));
    } else {
      new_store = backing_store_;
    }
    uint8_t* src = backing_store_.start();
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

  DISALLOW_COPY_AND_ASSIGN(LiteralBuffer);
};


// ----------------------------------------------------------------------------
// JavaScript Scanner.

class Scanner {
 public:
  // Scoped helper for literal recording. Automatically drops the literal
  // if aborting the scanning before it's complete.
  class LiteralScope {
   public:
    explicit LiteralScope(Scanner* self)
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
    Scanner* scanner_;
    bool complete_;
  };

  // Representation of an interval of source positions.
  struct Location {
    Location(int b, int e) : beg_pos(b), end_pos(e) { }
    Location() : beg_pos(0), end_pos(0) { }

    bool IsValid() const {
      return beg_pos >= 0 && end_pos >= beg_pos;
    }

    static Location invalid() { return Location(-1, -1); }

    int beg_pos;
    int end_pos;
  };

  // -1 is outside of the range of any real source code.
  static const int kNoOctalLocation = -1;

  explicit Scanner(UnicodeCache* scanner_contants);

  void Initialize(Utf16CharacterStream* source);

  // Returns the next token and advances input.
  Token::Value Next();
  // Returns the current token again.
  Token::Value current_token() { return current_.token; }
  // Returns the location information for the current token
  // (the token last returned by Next()).
  Location location() const { return current_.location; }
  // Returns the literal string, if any, for the current token (the
  // token last returned by Next()). The string is 0-terminated.
  // Literal strings are collected for identifiers, strings, and
  // numbers.
  // These functions only give the correct result if the literal
  // was scanned between calls to StartLiteral() and TerminateLiteral().
  Vector<const char> literal_ascii_string() {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->ascii_literal();
  }
  Vector<const uc16> literal_utf16_string() {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->utf16_literal();
  }
  bool is_literal_ascii() {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->is_ascii();
  }
  bool is_literal_contextual_keyword(Vector<const char> keyword) {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->is_contextual_keyword(keyword);
  }
  int literal_length() const {
    ASSERT_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->length();
  }

  bool literal_contains_escapes() const {
    Location location = current_.location;
    int source_length = (location.end_pos - location.beg_pos);
    if (current_.token == Token::STRING) {
      // Subtract delimiters.
      source_length -= 2;
    }
    return current_.literal_chars->length() != source_length;
  }

  // Similar functions for the upcoming token.

  // One token look-ahead (past the token returned by Next()).
  Token::Value peek() const { return next_.token; }

  Location peek_location() const { return next_.location; }

  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  Vector<const char> next_literal_ascii_string() {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->ascii_literal();
  }
  Vector<const uc16> next_literal_utf16_string() {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->utf16_literal();
  }
  bool is_next_literal_ascii() {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->is_ascii();
  }
  bool is_next_contextual_keyword(Vector<const char> keyword) {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->is_contextual_keyword(keyword);
  }
  int next_literal_length() const {
    ASSERT_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->length();
  }

  UnicodeCache* unicode_cache() { return unicode_cache_; }

  static const int kCharacterLookaheadBufferSize = 1;

  // Scans octal escape sequence. Also accepts "\0" decimal escape sequence.
  uc32 ScanOctalEscape(uc32 c, int length);

  // Returns the location of the last seen octal literal.
  Location octal_position() const { return octal_pos_; }
  void clear_octal_position() { octal_pos_ = Location::invalid(); }

  // Seek forward to the given position.  This operation does not
  // work in general, for instance when there are pushed back
  // characters, but works for seeking forward until simple delimiter
  // tokens, which is what it is used for.
  void SeekForward(int pos);

  bool HarmonyScoping() const {
    return harmony_scoping_;
  }
  void SetHarmonyScoping(bool scoping) {
    harmony_scoping_ = scoping;
  }
  bool HarmonyModules() const {
    return harmony_modules_;
  }
  void SetHarmonyModules(bool modules) {
    harmony_modules_ = modules;
  }


  // Returns true if there was a line terminator before the peek'ed token,
  // possibly inside a multi-line comment.
  bool HasAnyLineTerminatorBeforeNext() const {
    return has_line_terminator_before_next_ ||
           has_multiline_comment_before_next_;
  }

  // Scans the input as a regular expression pattern, previous
  // character(s) must be /(=). Returns true if a pattern is scanned.
  bool ScanRegExpPattern(bool seen_equal);
  // Returns true if regexp flags are scanned (always since flags can
  // be empty).
  bool ScanRegExpFlags();

 private:
  // The current and look-ahead token.
  struct TokenDesc {
    Token::Value token;
    Location location;
    LiteralBuffer* literal_chars;
  };

  // Call this after setting source_ to the input.
  void Init() {
    // Set c0_ (one character ahead)
    STATIC_ASSERT(kCharacterLookaheadBufferSize == 1);
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

  INLINE(void AddLiteralChar(uc32 c)) {
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

  uc32 ScanHexNumber(int expected_length);

  // Scans a single JavaScript token.
  void Scan();

  bool SkipWhiteSpace();
  Token::Value SkipSingleLineComment();
  Token::Value SkipMultiLineComment();
  // Scans a possible HTML comment -- begins with '<!'.
  Token::Value ScanHtmlComment();

  void ScanDecimalDigits();
  Token::Value ScanNumber(bool seen_period);
  Token::Value ScanIdentifierOrKeyword();
  Token::Value ScanIdentifierSuffix(LiteralScope* literal);

  Token::Value ScanString();

  // Scans an escape-sequence which is part of a string and adds the
  // decoded character to the current literal. Returns true if a pattern
  // is scanned.
  bool ScanEscape();
  // Decodes a Unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadChar.
  uc32 ScanIdentifierUnicodeEscape();
  // Scans a Unicode escape-sequence and adds its characters,
  // uninterpreted, to the current literal. Used for parsing RegExp
  // flags.
  bool ScanLiteralUnicodeEscape();

  // Return the current source position.
  int source_pos() {
    return source_->pos() - kCharacterLookaheadBufferSize;
  }

  UnicodeCache* unicode_cache_;

  // Buffers collecting literal strings, numbers, etc.
  LiteralBuffer literal_buffer1_;
  LiteralBuffer literal_buffer2_;

  TokenDesc current_;  // desc for current token (as returned by Next())
  TokenDesc next_;     // desc for next token (one token look-ahead)

  // Input stream. Must be initialized to an Utf16CharacterStream.
  Utf16CharacterStream* source_;


  // Start position of the octal literal last scanned.
  Location octal_pos_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;

  // Whether there is a line terminator whitespace character after
  // the current token, and  before the next. Does not count newlines
  // inside multiline comments.
  bool has_line_terminator_before_next_;
  // Whether there is a multi-line comment that contains a
  // line-terminator after the current token, and before the next.
  bool has_multiline_comment_before_next_;
  // Whether we scan 'let' as a keyword for harmony block-scoped let bindings.
  bool harmony_scoping_;
  // Whether we scan 'module', 'import', 'export' as keywords.
  bool harmony_modules_;
};

} }  // namespace v8::internal

#endif  // V8_SCANNER_H_
