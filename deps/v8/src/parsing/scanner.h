// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Features shared by parsing and pre-parsing scanners.

#ifndef V8_PARSING_SCANNER_H_
#define V8_PARSING_SCANNER_H_

#include "src/allocation.h"
#include "src/base/logging.h"
#include "src/char-predicates.h"
#include "src/globals.h"
#include "src/messages.h"
#include "src/parsing/token.h"
#include "src/unicode-decoder.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {


class AstRawString;
class AstValueFactory;
class DuplicateFinder;
class ExternalOneByteString;
class ExternalTwoByteString;
class ParserRecorder;
class UnicodeCache;

// ---------------------------------------------------------------------
// Buffered stream of UTF-16 code units, using an internal UTF-16 buffer.
// A code unit is a 16 bit value representing either a 16 bit code point
// or one part of a surrogate pair that make a single 21 bit code point.
class Utf16CharacterStream {
 public:
  static const uc32 kEndOfInput = -1;

  virtual ~Utf16CharacterStream() { }

  // Returns and advances past the next UTF-16 code unit in the input
  // stream. If there are no more code units it returns kEndOfInput.
  inline uc32 Advance() {
    if (V8_LIKELY(buffer_cursor_ < buffer_end_)) {
      return static_cast<uc32>(*(buffer_cursor_++));
    } else if (ReadBlock()) {
      return static_cast<uc32>(*(buffer_cursor_++));
    } else {
      // Note: currently the following increment is necessary to avoid a
      // parser problem! The scanner treats the final kEndOfInput as
      // a code unit with a position, and does math relative to that
      // position.
      buffer_cursor_++;
      return kEndOfInput;
    }
  }

  // Go back one by one character in the input stream.
  // This undoes the most recent Advance().
  inline void Back() {
    // The common case - if the previous character is within
    // buffer_start_ .. buffer_end_ will be handles locally.
    // Otherwise, a new block is requested.
    if (V8_LIKELY(buffer_cursor_ > buffer_start_)) {
      buffer_cursor_--;
    } else {
      ReadBlockAt(pos() - 1);
    }
  }

  // Go back one by two characters in the input stream. (This is the same as
  // calling Back() twice. But Back() may - in some instances - do substantial
  // work. Back2() guarantees this work will be done only once.)
  inline void Back2() {
    if (V8_LIKELY(buffer_cursor_ - 2 >= buffer_start_)) {
      buffer_cursor_ -= 2;
    } else {
      ReadBlockAt(pos() - 2);
    }
  }

  inline size_t pos() const {
    return buffer_pos_ + (buffer_cursor_ - buffer_start_);
  }

  inline void Seek(size_t pos) {
    if (V8_LIKELY(pos >= buffer_pos_ &&
                  pos < (buffer_pos_ + (buffer_end_ - buffer_start_)))) {
      buffer_cursor_ = buffer_start_ + (pos - buffer_pos_);
    } else {
      ReadBlockAt(pos);
    }
  }

 protected:
  Utf16CharacterStream(const uint16_t* buffer_start,
                       const uint16_t* buffer_cursor,
                       const uint16_t* buffer_end, size_t buffer_pos)
      : buffer_start_(buffer_start),
        buffer_cursor_(buffer_cursor),
        buffer_end_(buffer_end),
        buffer_pos_(buffer_pos) {}
  Utf16CharacterStream() : Utf16CharacterStream(nullptr, nullptr, nullptr, 0) {}

  void ReadBlockAt(size_t new_pos) {
    // The callers of this method (Back/Back2/Seek) should handle the easy
    // case (seeking within the current buffer), and we should only get here
    // if we actually require new data.
    // (This is really an efficiency check, not a correctness invariant.)
    DCHECK(new_pos < buffer_pos_ ||
           new_pos >= buffer_pos_ + (buffer_end_ - buffer_start_));

    // Change pos() to point to new_pos.
    buffer_pos_ = new_pos;
    buffer_cursor_ = buffer_start_;
    bool success = ReadBlock();
    USE(success);

    // Post-conditions: 1, on success, we should be at the right position.
    //                  2, success == we should have more characters available.
    DCHECK_IMPLIES(success, pos() == new_pos);
    DCHECK_EQ(success, buffer_cursor_ < buffer_end_);
    DCHECK_EQ(success, buffer_start_ < buffer_end_);
  }

  // Read more data, and update buffer_*_ to point to it.
  // Returns true if more data was available.
  //
  // ReadBlock() may modify any of the buffer_*_ members, but must sure that
  // the result of pos() remains unaffected.
  //
  // Examples:
  // - a stream could either fill a separate buffer. Then buffer_start_ and
  //   buffer_cursor_ would point to the beginning of the buffer, and
  //   buffer_pos would be the old pos().
  // - a stream with existing buffer chunks would set buffer_start_ and
  //   buffer_end_ to cover the full chunk, and then buffer_cursor_ would
  //   point into the middle of the buffer, while buffer_pos_ would describe
  //   the start of the buffer.
  virtual bool ReadBlock() = 0;

  const uint16_t* buffer_start_;
  const uint16_t* buffer_cursor_;
  const uint16_t* buffer_end_;
  size_t buffer_pos_;
};


// ----------------------------------------------------------------------------
// JavaScript Scanner.

class Scanner {
 public:
  // Scoped helper for a re-settable bookmark.
  class BookmarkScope {
   public:
    explicit BookmarkScope(Scanner* scanner)
        : scanner_(scanner), bookmark_(kNoBookmark) {
      DCHECK_NOT_NULL(scanner_);
    }
    ~BookmarkScope() {}

    void Set();
    void Apply();
    bool HasBeenSet();
    bool HasBeenApplied();

   private:
    static const size_t kNoBookmark;
    static const size_t kBookmarkWasApplied;
    static const size_t kBookmarkAtFirstPos;

    Scanner* scanner_;
    size_t bookmark_;

    DISALLOW_COPY_AND_ASSIGN(BookmarkScope);
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
  static const uc32 kEndOfInput = Utf16CharacterStream::kEndOfInput;

  explicit Scanner(UnicodeCache* scanner_contants);

  void Initialize(Utf16CharacterStream* source, bool is_module);

  // Returns the next token and advances input.
  Token::Value Next();
  // Returns the token following peek()
  Token::Value PeekAhead();
  // Returns the current token again.
  Token::Value current_token() { return current_.token; }

  Token::Value current_contextual_token() { return current_.contextual_token; }
  Token::Value next_contextual_token() { return next_.contextual_token; }

  // Returns the location information for the current token
  // (the token last returned by Next()).
  Location location() const { return current_.location; }

  // This error is specifically an invalid hex or unicode escape sequence.
  bool has_error() const { return scanner_error_ != MessageTemplate::kNone; }
  MessageTemplate::Template error() const { return scanner_error_; }
  Location error_location() const { return scanner_error_location_; }

  bool has_invalid_template_escape() const {
    return current_.invalid_template_escape_message != MessageTemplate::kNone;
  }
  MessageTemplate::Template invalid_template_escape_message() const {
    DCHECK(has_invalid_template_escape());
    return current_.invalid_template_escape_message;
  }
  Location invalid_template_escape_location() const {
    DCHECK(has_invalid_template_escape());
    return current_.invalid_template_escape_location;
  }

  // Similar functions for the upcoming token.

  // One token look-ahead (past the token returned by Next()).
  Token::Value peek() const { return next_.token; }

  Location peek_location() const { return next_.location; }

  bool literal_contains_escapes() const {
    return LiteralContainsEscapes(current_);
  }

  const AstRawString* CurrentSymbol(AstValueFactory* ast_value_factory) const;
  const AstRawString* NextSymbol(AstValueFactory* ast_value_factory) const;
  const AstRawString* CurrentRawSymbol(
      AstValueFactory* ast_value_factory) const;

  double DoubleValue();

  inline bool CurrentMatches(Token::Value token) const {
    DCHECK(Token::IsKeyword(token));
    return current_.token == token;
  }

  inline bool CurrentMatchesContextual(Token::Value token) const {
    DCHECK(Token::IsContextualKeyword(token));
    return current_.contextual_token == token;
  }

  // Match the token against the contextual keyword or literal buffer.
  inline bool CurrentMatchesContextualEscaped(Token::Value token) const {
    DCHECK(Token::IsContextualKeyword(token) || token == Token::LET);
    // Escaped keywords are not matched as tokens. So if we require escape
    // and/or string processing we need to look at the literal content
    // (which was escape-processed already).
    // Conveniently, current_.literal_chars == nullptr for all proper keywords,
    // so this second condition should exit early in common cases.
    return (current_.contextual_token == token) ||
           (current_.literal_chars &&
            current_.literal_chars->Equals(Vector<const char>(
                Token::String(token), Token::StringLength(token))));
  }

  bool IsUseStrict() const {
    return current_.token == Token::STRING &&
           current_.literal_chars->Equals(
               Vector<const char>("use strict", strlen("use strict")));
  }
  bool IsGetOrSet(bool* is_get, bool* is_set) const {
    *is_get = CurrentMatchesContextual(Token::GET);
    *is_set = CurrentMatchesContextual(Token::SET);
    return *is_get || *is_set;
  }
  bool IsLet() const {
    return CurrentMatches(Token::LET) ||
           CurrentMatchesContextualEscaped(Token::LET);
  }

  // Check whether the CurrentSymbol() has already been seen.
  // The DuplicateFinder holds the data, so different instances can be used
  // for different sets of duplicates to check for.
  bool IsDuplicateSymbol(DuplicateFinder* duplicate_finder,
                         AstValueFactory* ast_value_factory) const;

  UnicodeCache* unicode_cache() { return unicode_cache_; }

  // Returns the location of the last seen octal literal.
  Location octal_position() const { return octal_pos_; }
  void clear_octal_position() {
    octal_pos_ = Location::invalid();
    octal_message_ = MessageTemplate::kNone;
  }
  MessageTemplate::Template octal_message() const { return octal_message_; }

  // Returns the value of the last smi that was scanned.
  uint32_t smi_value() const { return current_.smi_value_; }

  // Seek forward to the given position.  This operation does not
  // work in general, for instance when there are pushed back
  // characters, but works for seeking forward until simple delimiter
  // tokens, which is what it is used for.
  void SeekForward(int pos);

  // Returns true if there was a line terminator before the peek'ed token,
  // possibly inside a multi-line comment.
  bool HasAnyLineTerminatorBeforeNext() const {
    return has_line_terminator_before_next_ ||
           has_multiline_comment_before_next_;
  }

  bool HasAnyLineTerminatorAfterNext() {
    Token::Value ensure_next_next = PeekAhead();
    USE(ensure_next_next);
    return has_line_terminator_after_next_;
  }

  // Scans the input as a regular expression pattern, next token must be /(=).
  // Returns true if a pattern is scanned.
  bool ScanRegExpPattern();
  // Scans the input as regular expression flags. Returns the flags on success.
  Maybe<RegExp::Flags> ScanRegExpFlags();

  // Scans the input as a template literal
  Token::Value ScanTemplateStart();
  Token::Value ScanTemplateContinuation();

  Handle<String> SourceUrl(Isolate* isolate) const;
  Handle<String> SourceMappingUrl(Isolate* isolate) const;

  bool FoundHtmlComment() const { return found_html_comment_; }

 private:
  // Scoped helper for saving & restoring scanner error state.
  // This is used for tagged template literals, in which normally forbidden
  // escape sequences are allowed.
  class ErrorState;

  // Scoped helper for literal recording. Automatically drops the literal
  // if aborting the scanning before it's complete.
  class LiteralScope {
   public:
    explicit LiteralScope(Scanner* self) : scanner_(self), complete_(false) {
      scanner_->StartLiteral();
    }
    ~LiteralScope() {
      if (!complete_) scanner_->DropLiteral();
    }
    void Complete() { complete_ = true; }

   private:
    Scanner* scanner_;
    bool complete_;
  };

  // LiteralBuffer -  Collector of chars of literals.
  class LiteralBuffer {
   public:
    LiteralBuffer() : is_one_byte_(true), position_(0), backing_store_() {}

    ~LiteralBuffer() { backing_store_.Dispose(); }

    INLINE(void AddChar(char code_unit)) {
      DCHECK(IsValidAscii(code_unit));
      AddOneByteChar(static_cast<byte>(code_unit));
    }

    INLINE(void AddChar(uc32 code_unit)) {
      if (is_one_byte_ &&
          code_unit <= static_cast<uc32>(unibrow::Latin1::kMaxChar)) {
        AddOneByteChar(static_cast<byte>(code_unit));
      } else {
        AddCharSlow(code_unit);
      }
    }

    bool is_one_byte() const { return is_one_byte_; }

    bool Equals(Vector<const char> keyword) const {
      return is_one_byte() && keyword.length() == position_ &&
             (memcmp(keyword.start(), backing_store_.start(), position_) == 0);
    }

    Vector<const uint16_t> two_byte_literal() const {
      DCHECK(!is_one_byte_);
      DCHECK((position_ & 0x1) == 0);
      return Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(backing_store_.start()),
          position_ >> 1);
    }

    Vector<const uint8_t> one_byte_literal() const {
      DCHECK(is_one_byte_);
      return Vector<const uint8_t>(
          reinterpret_cast<const uint8_t*>(backing_store_.start()), position_);
    }

    int length() const { return is_one_byte_ ? position_ : (position_ >> 1); }

    void ReduceLength(int delta) {
      position_ -= delta * (is_one_byte_ ? kOneByteSize : kUC16Size);
    }

    void Reset() {
      position_ = 0;
      is_one_byte_ = true;
    }

    Handle<String> Internalize(Isolate* isolate) const;

   private:
    static const int kInitialCapacity = 16;
    static const int kGrowthFactory = 4;
    static const int kMinConversionSlack = 256;
    static const int kMaxGrowth = 1 * MB;

    inline bool IsValidAscii(char code_unit) {
      // Control characters and printable characters span the range of
      // valid ASCII characters (0-127). Chars are unsigned on some
      // platforms which causes compiler warnings if the validity check
      // tests the lower bound >= 0 as it's always true.
      return iscntrl(code_unit) || isprint(code_unit);
    }

    INLINE(void AddOneByteChar(byte one_byte_char)) {
      DCHECK(is_one_byte_);
      if (position_ >= backing_store_.length()) ExpandBuffer();
      backing_store_[position_] = one_byte_char;
      position_ += kOneByteSize;
    }

    void AddCharSlow(uc32 code_unit);
    int NewCapacity(int min_capacity);
    void ExpandBuffer();
    void ConvertToTwoByte();

    bool is_one_byte_;
    int position_;
    Vector<byte> backing_store_;

    DISALLOW_COPY_AND_ASSIGN(LiteralBuffer);
  };

  // The current and look-ahead token.
  struct TokenDesc {
    Location location;
    LiteralBuffer* literal_chars;
    LiteralBuffer* raw_literal_chars;
    uint32_t smi_value_;
    Token::Value token;
    MessageTemplate::Template invalid_template_escape_message;
    Location invalid_template_escape_location;
    Token::Value contextual_token;
  };

  static const int kCharacterLookaheadBufferSize = 1;
  const int kMaxAscii = 127;

  // Scans octal escape sequence. Also accepts "\0" decimal escape sequence.
  template <bool capture_raw>
  uc32 ScanOctalEscape(uc32 c, int length);

  // Call this after setting source_ to the input.
  void Init() {
    // Set c0_ (one character ahead)
    STATIC_ASSERT(kCharacterLookaheadBufferSize == 1);
    Advance();
    // Initialize current_ to not refer to a literal.
    current_.token = Token::UNINITIALIZED;
    current_.contextual_token = Token::UNINITIALIZED;
    current_.literal_chars = NULL;
    current_.raw_literal_chars = NULL;
    current_.invalid_template_escape_message = MessageTemplate::kNone;
    next_.token = Token::UNINITIALIZED;
    next_.contextual_token = Token::UNINITIALIZED;
    next_.literal_chars = NULL;
    next_.raw_literal_chars = NULL;
    next_.invalid_template_escape_message = MessageTemplate::kNone;
    next_next_.token = Token::UNINITIALIZED;
    next_next_.contextual_token = Token::UNINITIALIZED;
    next_next_.literal_chars = NULL;
    next_next_.raw_literal_chars = NULL;
    next_next_.invalid_template_escape_message = MessageTemplate::kNone;
    found_html_comment_ = false;
    scanner_error_ = MessageTemplate::kNone;
  }

  void ReportScannerError(const Location& location,
                          MessageTemplate::Template error) {
    if (has_error()) return;
    scanner_error_ = error;
    scanner_error_location_ = location;
  }

  void ReportScannerError(int pos, MessageTemplate::Template error) {
    if (has_error()) return;
    scanner_error_ = error;
    scanner_error_location_ = Location(pos, pos + 1);
  }

  // Seek to the next_ token at the given position.
  void SeekNext(size_t position);

  // Literal buffer support
  inline void StartLiteral() {
    LiteralBuffer* free_buffer =
        (current_.literal_chars == &literal_buffer0_)
            ? &literal_buffer1_
            : (current_.literal_chars == &literal_buffer1_) ? &literal_buffer2_
                                                            : &literal_buffer0_;
    free_buffer->Reset();
    next_.literal_chars = free_buffer;
  }

  inline void StartRawLiteral() {
    LiteralBuffer* free_buffer =
        (current_.raw_literal_chars == &raw_literal_buffer0_)
            ? &raw_literal_buffer1_
            : (current_.raw_literal_chars == &raw_literal_buffer1_)
                  ? &raw_literal_buffer2_
                  : &raw_literal_buffer0_;
    free_buffer->Reset();
    next_.raw_literal_chars = free_buffer;
  }

  INLINE(void AddLiteralChar(uc32 c)) {
    DCHECK_NOT_NULL(next_.literal_chars);
    next_.literal_chars->AddChar(c);
  }

  INLINE(void AddLiteralChar(char c)) {
    DCHECK_NOT_NULL(next_.literal_chars);
    next_.literal_chars->AddChar(c);
  }

  INLINE(void AddRawLiteralChar(uc32 c)) {
    DCHECK_NOT_NULL(next_.raw_literal_chars);
    next_.raw_literal_chars->AddChar(c);
  }

  INLINE(void ReduceRawLiteralLength(int delta)) {
    DCHECK_NOT_NULL(next_.raw_literal_chars);
    next_.raw_literal_chars->ReduceLength(delta);
  }

  // Stops scanning of a literal and drop the collected characters,
  // e.g., due to an encountered error.
  inline void DropLiteral() {
    next_.literal_chars = NULL;
    next_.raw_literal_chars = NULL;
  }

  inline void AddLiteralCharAdvance() {
    AddLiteralChar(c0_);
    Advance();
  }

  // Low-level scanning support.
  template <bool capture_raw = false, bool check_surrogate = true>
  void Advance() {
    if (capture_raw) {
      AddRawLiteralChar(c0_);
    }
    c0_ = source_->Advance();
    if (check_surrogate) HandleLeadSurrogate();
  }

  void HandleLeadSurrogate() {
    if (unibrow::Utf16::IsLeadSurrogate(c0_)) {
      uc32 c1 = source_->Advance();
      if (!unibrow::Utf16::IsTrailSurrogate(c1)) {
        source_->Back();
      } else {
        c0_ = unibrow::Utf16::CombineSurrogatePair(c0_, c1);
      }
    }
  }

  void PushBack(uc32 ch) {
    if (c0_ > static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
      source_->Back2();
    } else {
      source_->Back();
    }
    c0_ = ch;
  }

  // Same as PushBack(ch1); PushBack(ch2).
  // - Potentially more efficient as it uses Back2() on the stream.
  // - Uses char as parameters, since we're only calling it with ASCII chars in
  //   practice. This way, we can avoid a few edge cases.
  void PushBack2(char ch1, char ch2) {
    source_->Back2();
    c0_ = ch2;
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
  // Returns the literal string, if any, for the current token (the
  // token last returned by Next()). The string is 0-terminated.
  // Literal strings are collected for identifiers, strings, numbers as well
  // as for template literals. For template literals we also collect the raw
  // form.
  // These functions only give the correct result if the literal was scanned
  // when a LiteralScope object is alive.
  //
  // Current usage of these functions is unfortunately a little undisciplined,
  // and is_literal_one_byte() + is_literal_one_byte_string() is also
  // requested for tokens that do not have a literal. Hence, we treat any
  // token as a one-byte literal. E.g. Token::FUNCTION pretends to have a
  // literal "function".
  Vector<const uint8_t> literal_one_byte_string() const {
    if (current_.literal_chars)
      return current_.literal_chars->one_byte_literal();
    const char* str = Token::String(current_.token);
    const uint8_t* str_as_uint8 = reinterpret_cast<const uint8_t*>(str);
    return Vector<const uint8_t>(str_as_uint8,
                                 Token::StringLength(current_.token));
  }
  Vector<const uint16_t> literal_two_byte_string() const {
    DCHECK_NOT_NULL(current_.literal_chars);
    return current_.literal_chars->two_byte_literal();
  }
  bool is_literal_one_byte() const {
    return !current_.literal_chars || current_.literal_chars->is_one_byte();
  }
  int literal_length() const {
    if (current_.literal_chars) return current_.literal_chars->length();
    return Token::StringLength(current_.token);
  }
  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  Vector<const uint8_t> next_literal_one_byte_string() const {
    DCHECK_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->one_byte_literal();
  }
  Vector<const uint16_t> next_literal_two_byte_string() const {
    DCHECK_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->two_byte_literal();
  }
  bool is_next_literal_one_byte() const {
    DCHECK_NOT_NULL(next_.literal_chars);
    return next_.literal_chars->is_one_byte();
  }
  Vector<const uint8_t> raw_literal_one_byte_string() const {
    DCHECK_NOT_NULL(current_.raw_literal_chars);
    return current_.raw_literal_chars->one_byte_literal();
  }
  Vector<const uint16_t> raw_literal_two_byte_string() const {
    DCHECK_NOT_NULL(current_.raw_literal_chars);
    return current_.raw_literal_chars->two_byte_literal();
  }
  bool is_raw_literal_one_byte() const {
    DCHECK_NOT_NULL(current_.raw_literal_chars);
    return current_.raw_literal_chars->is_one_byte();
  }

  template <bool capture_raw, bool unicode = false>
  uc32 ScanHexNumber(int expected_length);
  // Scan a number of any length but not bigger than max_value. For example, the
  // number can be 000000001, so it's very long in characters but its value is
  // small.
  template <bool capture_raw>
  uc32 ScanUnlimitedLengthHexNumber(int max_value, int beg_pos);

  // Scans a single JavaScript token.
  void Scan();

  Token::Value SkipWhiteSpace();
  Token::Value SkipSingleHTMLComment();
  Token::Value SkipSingleLineComment();
  Token::Value SkipSourceURLComment();
  void TryToParseSourceURLComment();
  Token::Value SkipMultiLineComment();
  // Scans a possible HTML comment -- begins with '<!'.
  Token::Value ScanHtmlComment();

  void ScanDecimalDigits();
  Token::Value ScanNumber(bool seen_period);
  Token::Value ScanIdentifierOrKeyword();
  Token::Value ScanIdentifierSuffix(LiteralScope* literal, bool escaped);

  Token::Value ScanString();

  // Scans an escape-sequence which is part of a string and adds the
  // decoded character to the current literal. Returns true if a pattern
  // is scanned.
  template <bool capture_raw, bool in_template_literal>
  bool ScanEscape();

  // Decodes a Unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadChar.
  uc32 ScanIdentifierUnicodeEscape();
  // Helper for the above functions.
  template <bool capture_raw>
  uc32 ScanUnicodeEscape();

  bool is_module_;

  Token::Value ScanTemplateSpan();

  // Return the current source position.
  int source_pos() {
    return static_cast<int>(source_->pos()) - kCharacterLookaheadBufferSize;
  }

  static bool LiteralContainsEscapes(const TokenDesc& token) {
    Location location = token.location;
    int source_length = (location.end_pos - location.beg_pos);
    if (token.token == Token::STRING) {
      // Subtract delimiters.
      source_length -= 2;
    }
    return token.literal_chars &&
           (token.literal_chars->length() != source_length);
  }

#ifdef DEBUG
  void SanityCheckTokenDesc(const TokenDesc&) const;
#endif

  UnicodeCache* unicode_cache_;

  // Buffers collecting literal strings, numbers, etc.
  LiteralBuffer literal_buffer0_;
  LiteralBuffer literal_buffer1_;
  LiteralBuffer literal_buffer2_;

  // Values parsed from magic comments.
  LiteralBuffer source_url_;
  LiteralBuffer source_mapping_url_;

  // Buffer to store raw string values
  LiteralBuffer raw_literal_buffer0_;
  LiteralBuffer raw_literal_buffer1_;
  LiteralBuffer raw_literal_buffer2_;

  TokenDesc current_;    // desc for current token (as returned by Next())
  TokenDesc next_;       // desc for next token (one token look-ahead)
  TokenDesc next_next_;  // desc for the token after next (after PeakAhead())

  // Input stream. Must be initialized to an Utf16CharacterStream.
  Utf16CharacterStream* source_;

  // Last-seen positions of potentially problematic tokens.
  Location octal_pos_;
  MessageTemplate::Template octal_message_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;

  // Whether there is a line terminator whitespace character after
  // the current token, and  before the next. Does not count newlines
  // inside multiline comments.
  bool has_line_terminator_before_next_;
  // Whether there is a multi-line comment that contains a
  // line-terminator after the current token, and before the next.
  bool has_multiline_comment_before_next_;
  bool has_line_terminator_after_next_;

  // Whether this scanner encountered an HTML comment.
  bool found_html_comment_;

  MessageTemplate::Template scanner_error_;
  Location scanner_error_location_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_SCANNER_H_
