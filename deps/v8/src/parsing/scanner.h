// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Features shared by parsing and pre-parsing scanners.

#ifndef V8_PARSING_SCANNER_H_
#define V8_PARSING_SCANNER_H_

#include <algorithm>

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
class RuntimeCallStats;
class UnicodeCache;

// ---------------------------------------------------------------------
// Buffered stream of UTF-16 code units, using an internal UTF-16 buffer.
// A code unit is a 16 bit value representing either a 16 bit code point
// or one part of a surrogate pair that make a single 21 bit code point.
class Utf16CharacterStream {
 public:
  static const uc32 kEndOfInput = -1;

  virtual ~Utf16CharacterStream() = default;

  inline uc32 Peek() {
    if (V8_LIKELY(buffer_cursor_ < buffer_end_)) {
      return static_cast<uc32>(*buffer_cursor_);
    } else if (ReadBlockChecked()) {
      return static_cast<uc32>(*buffer_cursor_);
    } else {
      return kEndOfInput;
    }
  }

  // Returns and advances past the next UTF-16 code unit in the input
  // stream. If there are no more code units it returns kEndOfInput.
  inline uc32 Advance() {
    uc32 result = Peek();
    buffer_cursor_++;
    return result;
  }

  // Returns and advances past the next UTF-16 code unit in the input stream
  // that meets the checks requirement. If there are no more code units it
  // returns kEndOfInput.
  template <typename FunctionType>
  V8_INLINE uc32 AdvanceUntil(FunctionType check) {
    while (true) {
      auto next_cursor_pos =
          std::find_if(buffer_cursor_, buffer_end_, [&check](uint16_t raw_c0_) {
            uc32 c0_ = static_cast<uc32>(raw_c0_);
            return check(c0_);
          });

      if (next_cursor_pos == buffer_end_) {
        buffer_cursor_ = buffer_end_;
        if (!ReadBlockChecked()) {
          buffer_cursor_++;
          return kEndOfInput;
        }
      } else {
        buffer_cursor_ = next_cursor_pos + 1;
        return static_cast<uc32>(*next_cursor_pos);
      }
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

  // Returns true if the stream can be cloned with Clone.
  // TODO(rmcilroy): Remove this once ChunkedStreams can be cloned.
  virtual bool can_be_cloned() const = 0;

  // Clones the character stream to enable another independent scanner to access
  // the same underlying stream.
  virtual std::unique_ptr<Utf16CharacterStream> Clone() const = 0;

  // Returns true if the stream could access the V8 heap after construction.
  virtual bool can_access_heap() const = 0;

  RuntimeCallStats* runtime_call_stats() const { return runtime_call_stats_; }
  void set_runtime_call_stats(RuntimeCallStats* runtime_call_stats) {
    runtime_call_stats_ = runtime_call_stats;
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

  bool ReadBlockChecked() {
    size_t position = pos();
    USE(position);
    bool success = ReadBlock();

    // Post-conditions: 1, We should always be at the right position.
    //                  2, Cursor should be inside the buffer.
    //                  3, We should have more characters available iff success.
    DCHECK_EQ(pos(), position);
    DCHECK_LE(buffer_cursor_, buffer_end_);
    DCHECK_LE(buffer_start_, buffer_cursor_);
    DCHECK_EQ(success, buffer_cursor_ < buffer_end_);
    return success;
  }

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
    DCHECK_EQ(pos(), new_pos);
    ReadBlockChecked();
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
  RuntimeCallStats* runtime_call_stats_;
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
    ~BookmarkScope() = default;

    void Set();
    void Apply();
    bool HasBeenSet() const;
    bool HasBeenApplied() const;

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

  explicit Scanner(UnicodeCache* scanner_contants, Utf16CharacterStream* source,
                   bool is_module);

  void Initialize();

  // Returns the next token and advances input.
  Token::Value Next();
  // Returns the token following peek()
  Token::Value PeekAhead();
  // Returns the current token again.
  Token::Value current_token() const { return current().token; }

  Token::Value current_contextual_token() const {
    return current().contextual_token;
  }
  Token::Value next_contextual_token() const { return next().contextual_token; }

  // Returns the location information for the current token
  // (the token last returned by Next()).
  const Location& location() const { return current().location; }

  // This error is specifically an invalid hex or unicode escape sequence.
  bool has_error() const { return scanner_error_ != MessageTemplate::kNone; }
  MessageTemplate::Template error() const { return scanner_error_; }
  const Location& error_location() const { return scanner_error_location_; }

  bool has_invalid_template_escape() const {
    return current().invalid_template_escape_message != MessageTemplate::kNone;
  }
  MessageTemplate::Template invalid_template_escape_message() const {
    DCHECK(has_invalid_template_escape());
    return current().invalid_template_escape_message;
  }
  Location invalid_template_escape_location() const {
    DCHECK(has_invalid_template_escape());
    return current().invalid_template_escape_location;
  }

  // Similar functions for the upcoming token.

  // One token look-ahead (past the token returned by Next()).
  Token::Value peek() const { return next().token; }

  const Location& peek_location() const { return next().location; }

  bool literal_contains_escapes() const {
    return LiteralContainsEscapes(current());
  }

  const AstRawString* CurrentSymbol(AstValueFactory* ast_value_factory) const;

  const AstRawString* NextSymbol(AstValueFactory* ast_value_factory) const;
  const AstRawString* CurrentRawSymbol(
      AstValueFactory* ast_value_factory) const;

  double DoubleValue();

  const char* CurrentLiteralAsCString(Zone* zone) const;

  inline bool CurrentMatches(Token::Value token) const {
    DCHECK(Token::IsKeyword(token));
    return current().token == token;
  }

  inline bool CurrentMatchesContextual(Token::Value token) const {
    DCHECK(Token::IsContextualKeyword(token));
    return current_contextual_token() == token;
  }

  // Match the token against the contextual keyword or literal buffer.
  inline bool CurrentMatchesContextualEscaped(Token::Value token) const {
    DCHECK(Token::IsContextualKeyword(token) || token == Token::LET);
    // Escaped keywords are not matched as tokens. So if we require escape
    // and/or string processing we need to look at the literal content
    // (which was escape-processed already).
    // Conveniently, !current().literal_chars.is_used() for all proper
    // keywords, so this second condition should exit early in common cases.
    return (current_contextual_token() == token) ||
           (current().literal_chars.is_used() &&
            current().literal_chars.Equals(Vector<const char>(
                Token::String(token), Token::StringLength(token))));
  }

  bool IsUseStrict() const {
    return current().token == Token::STRING &&
           current().literal_chars.Equals(
               Vector<const char>("use strict", strlen("use strict")));
  }

  bool IsGet() { return CurrentMatchesContextual(Token::GET); }

  bool IsSet() { return CurrentMatchesContextual(Token::SET); }

  bool IsLet() const {
    return CurrentMatches(Token::LET) ||
           CurrentMatchesContextualEscaped(Token::LET);
  }

  // Check whether the CurrentSymbol() has already been seen.
  // The DuplicateFinder holds the data, so different instances can be used
  // for different sets of duplicates to check for.
  bool IsDuplicateSymbol(DuplicateFinder* duplicate_finder,
                         AstValueFactory* ast_value_factory) const;

  UnicodeCache* unicode_cache() const { return unicode_cache_; }

  // Returns the location of the last seen octal literal.
  Location octal_position() const { return octal_pos_; }
  void clear_octal_position() {
    octal_pos_ = Location::invalid();
    octal_message_ = MessageTemplate::kNone;
  }
  MessageTemplate::Template octal_message() const { return octal_message_; }

  // Returns the value of the last smi that was scanned.
  uint32_t smi_value() const { return current().smi_value_; }

  // Seek forward to the given position.  This operation does not
  // work in general, for instance when there are pushed back
  // characters, but works for seeking forward until simple delimiter
  // tokens, which is what it is used for.
  void SeekForward(int pos);

  // Returns true if there was a line terminator before the peek'ed token,
  // possibly inside a multi-line comment.
  bool HasLineTerminatorBeforeNext() const {
    return next().after_line_terminator;
  }

  bool HasLineTerminatorAfterNext() {
    Token::Value ensure_next_next = PeekAhead();
    USE(ensure_next_next);
    return next_next().after_line_terminator;
  }

  // Scans the input as a regular expression pattern, next token must be /(=).
  // Returns true if a pattern is scanned.
  bool ScanRegExpPattern();
  // Scans the input as regular expression flags. Returns the flags on success.
  Maybe<RegExp::Flags> ScanRegExpFlags();

  // Scans the input as a template literal
  Token::Value ScanTemplateContinuation() {
    DCHECK_EQ(next().token, Token::RBRACE);
    DCHECK_EQ(source_pos() - 1, next().location.beg_pos);
    return ScanTemplateSpan();
  }

  Handle<String> SourceUrl(Isolate* isolate) const;
  Handle<String> SourceMappingUrl(Isolate* isolate) const;

  bool FoundHtmlComment() const { return found_html_comment_; }

  bool allow_harmony_private_fields() const {
    return allow_harmony_private_fields_;
  }
  void set_allow_harmony_private_fields(bool allow) {
    allow_harmony_private_fields_ = allow;
  }
  bool allow_harmony_numeric_separator() const {
    return allow_harmony_numeric_separator_;
  }
  void set_allow_harmony_numeric_separator(bool allow) {
    allow_harmony_numeric_separator_ = allow;
  }

  const Utf16CharacterStream* stream() const { return source_; }

 private:
  // Scoped helper for saving & restoring scanner error state.
  // This is used for tagged template literals, in which normally forbidden
  // escape sequences are allowed.
  class ErrorState;

  // LiteralBuffer -  Collector of chars of literals.
  class LiteralBuffer {
   public:
    LiteralBuffer()
        : backing_store_(), position_(0), is_one_byte_(true), is_used_(false) {}

    ~LiteralBuffer() { backing_store_.Dispose(); }

    V8_INLINE void AddChar(char code_unit) {
      DCHECK(is_used_);
      DCHECK(IsValidAscii(code_unit));
      AddOneByteChar(static_cast<byte>(code_unit));
    }

    V8_INLINE void AddChar(uc32 code_unit) {
      DCHECK(is_used_);
      if (is_one_byte_) {
        if (code_unit <= static_cast<uc32>(unibrow::Latin1::kMaxChar)) {
          AddOneByteChar(static_cast<byte>(code_unit));
          return;
        }
        ConvertToTwoByte();
      }
      AddTwoByteChar(code_unit);
    }

    bool is_one_byte() const { return is_one_byte_; }

    bool Equals(Vector<const char> keyword) const {
      DCHECK(is_used_);
      return is_one_byte() && keyword.length() == position_ &&
             (memcmp(keyword.start(), backing_store_.start(), position_) == 0);
    }

    Vector<const uint16_t> two_byte_literal() const {
      DCHECK(!is_one_byte_);
      DCHECK(is_used_);
      DCHECK_EQ(position_ & 0x1, 0);
      return Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(backing_store_.start()),
          position_ >> 1);
    }

    Vector<const uint8_t> one_byte_literal() const {
      DCHECK(is_one_byte_);
      DCHECK(is_used_);
      return Vector<const uint8_t>(
          reinterpret_cast<const uint8_t*>(backing_store_.start()), position_);
    }

    int length() const { return is_one_byte_ ? position_ : (position_ >> 1); }

    void Start() {
      DCHECK(!is_used_);
      DCHECK_EQ(0, position_);
      is_used_ = true;
    }

    bool is_used() const { return is_used_; }

    void Drop() {
      is_used_ = false;
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

    V8_INLINE void AddOneByteChar(byte one_byte_char) {
      DCHECK(is_one_byte_);
      if (position_ >= backing_store_.length()) ExpandBuffer();
      backing_store_[position_] = one_byte_char;
      position_ += kOneByteSize;
    }

    void AddTwoByteChar(uc32 code_unit);
    int NewCapacity(int min_capacity);
    void ExpandBuffer();
    void ConvertToTwoByte();

    Vector<byte> backing_store_;
    int position_;
    bool is_one_byte_;
    bool is_used_;

    DISALLOW_COPY_AND_ASSIGN(LiteralBuffer);
  };

  // Scoped helper for literal recording. Automatically drops the literal
  // if aborting the scanning before it's complete.
  class LiteralScope {
   public:
    explicit LiteralScope(Scanner* scanner)
        : buffer_(&scanner->next().literal_chars), complete_(false) {
      buffer_->Start();
    }
    ~LiteralScope() {
      if (!complete_) buffer_->Drop();
    }
    void Complete() { complete_ = true; }

   private:
    LiteralBuffer* buffer_;
    bool complete_;
  };

  // The current and look-ahead token.
  struct TokenDesc {
    Location location = {0, 0};
    LiteralBuffer literal_chars;
    LiteralBuffer raw_literal_chars;
    Token::Value token = Token::UNINITIALIZED;
    MessageTemplate::Template invalid_template_escape_message =
        MessageTemplate::kNone;
    Location invalid_template_escape_location;
    Token::Value contextual_token = Token::UNINITIALIZED;
    uint32_t smi_value_ = 0;
    bool after_line_terminator = false;
  };

  enum NumberKind {
    BINARY,
    OCTAL,
    IMPLICIT_OCTAL,
    HEX,
    DECIMAL,
    DECIMAL_WITH_LEADING_ZERO
  };

  static const int kCharacterLookaheadBufferSize = 1;
  static const int kMaxAscii = 127;

  // Scans octal escape sequence. Also accepts "\0" decimal escape sequence.
  template <bool capture_raw>
  uc32 ScanOctalEscape(uc32 c, int length);

  // Call this after setting source_ to the input.
  void Init() {
    // Set c0_ (one character ahead)
    STATIC_ASSERT(kCharacterLookaheadBufferSize == 1);
    Advance();

    current_ = &token_storage_[0];
    next_ = &token_storage_[1];
    next_next_ = &token_storage_[2];

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

  V8_INLINE void AddLiteralChar(uc32 c) { next().literal_chars.AddChar(c); }

  V8_INLINE void AddLiteralChar(char c) { next().literal_chars.AddChar(c); }

  V8_INLINE void AddRawLiteralChar(uc32 c) {
    next().raw_literal_chars.AddChar(c);
  }

  V8_INLINE void AddLiteralCharAdvance() {
    AddLiteralChar(c0_);
    Advance();
  }

  // Low-level scanning support.
  template <bool capture_raw = false>
  void Advance() {
    if (capture_raw) {
      AddRawLiteralChar(c0_);
    }
    c0_ = source_->Advance();
  }

  template <typename FunctionType>
  V8_INLINE void AdvanceUntil(FunctionType check) {
    c0_ = source_->AdvanceUntil(check);
  }

  bool CombineSurrogatePair() {
    DCHECK(!unibrow::Utf16::IsLeadSurrogate(kEndOfInput));
    if (unibrow::Utf16::IsLeadSurrogate(c0_)) {
      uc32 c1 = source_->Advance();
      DCHECK(!unibrow::Utf16::IsTrailSurrogate(kEndOfInput));
      if (unibrow::Utf16::IsTrailSurrogate(c1)) {
        c0_ = unibrow::Utf16::CombineSurrogatePair(c0_, c1);
        return true;
      }
      source_->Back();
    }
    return false;
  }

  void PushBack(uc32 ch) {
    DCHECK_LE(c0_, static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode));
    source_->Back();
    c0_ = ch;
  }

  uc32 Peek() const { return source_->Peek(); }

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
    if (current().literal_chars.is_used())
      return current().literal_chars.one_byte_literal();
    const char* str = Token::String(current().token);
    const uint8_t* str_as_uint8 = reinterpret_cast<const uint8_t*>(str);
    return Vector<const uint8_t>(str_as_uint8,
                                 Token::StringLength(current().token));
  }
  Vector<const uint16_t> literal_two_byte_string() const {
    DCHECK(current().literal_chars.is_used());
    return current().literal_chars.two_byte_literal();
  }
  bool is_literal_one_byte() const {
    return !current().literal_chars.is_used() ||
           current().literal_chars.is_one_byte();
  }
  // Returns the literal string for the next token (the token that
  // would be returned if Next() were called).
  Vector<const uint8_t> next_literal_one_byte_string() const {
    DCHECK(next().literal_chars.is_used());
    return next().literal_chars.one_byte_literal();
  }
  Vector<const uint16_t> next_literal_two_byte_string() const {
    DCHECK(next().literal_chars.is_used());
    return next().literal_chars.two_byte_literal();
  }
  bool is_next_literal_one_byte() const {
    DCHECK(next().literal_chars.is_used());
    return next().literal_chars.is_one_byte();
  }
  Vector<const uint8_t> raw_literal_one_byte_string() const {
    DCHECK(current().raw_literal_chars.is_used());
    return current().raw_literal_chars.one_byte_literal();
  }
  Vector<const uint16_t> raw_literal_two_byte_string() const {
    DCHECK(current().raw_literal_chars.is_used());
    return current().raw_literal_chars.two_byte_literal();
  }
  bool is_raw_literal_one_byte() const {
    DCHECK(current().raw_literal_chars.is_used());
    return current().raw_literal_chars.is_one_byte();
  }

  template <bool capture_raw, bool unicode = false>
  uc32 ScanHexNumber(int expected_length);
  // Scan a number of any length but not bigger than max_value. For example, the
  // number can be 000000001, so it's very long in characters but its value is
  // small.
  template <bool capture_raw>
  uc32 ScanUnlimitedLengthHexNumber(int max_value, int beg_pos);

  // Scans a single JavaScript token.
  V8_INLINE Token::Value ScanSingleToken();
  V8_INLINE void Scan();

  V8_INLINE Token::Value SkipWhiteSpace();
  Token::Value SkipSingleHTMLComment();
  Token::Value SkipSingleLineComment();
  Token::Value SkipSourceURLComment();
  void TryToParseSourceURLComment();
  Token::Value SkipMultiLineComment();
  // Scans a possible HTML comment -- begins with '<!'.
  Token::Value ScanHtmlComment();

  bool ScanDigitsWithNumericSeparators(bool (*predicate)(uc32 ch),
                                       bool is_check_first_digit);
  bool ScanDecimalDigits();
  // Optimized function to scan decimal number as Smi.
  bool ScanDecimalAsSmi(uint64_t* value);
  bool ScanDecimalAsSmiWithNumericSeparators(uint64_t* value);
  bool ScanHexDigits();
  bool ScanBinaryDigits();
  bool ScanSignedInteger();
  bool ScanOctalDigits();
  bool ScanImplicitOctalDigits(int start_pos, NumberKind* kind);

  Token::Value ScanNumber(bool seen_period);
  V8_INLINE Token::Value ScanIdentifierOrKeyword();
  V8_INLINE Token::Value ScanIdentifierOrKeywordInner(LiteralScope* literal);
  Token::Value ScanIdentifierOrKeywordInnerSlow(LiteralScope* literal,
                                                bool escaped);

  Token::Value ScanString();
  Token::Value ScanPrivateName();

  // Scans an escape-sequence which is part of a string and adds the
  // decoded character to the current literal. Returns true if a pattern
  // is scanned.
  template <bool capture_raw>
  bool ScanEscape();

  // Decodes a Unicode escape-sequence which is part of an identifier.
  // If the escape sequence cannot be decoded the result is kBadChar.
  uc32 ScanIdentifierUnicodeEscape();
  // Helper for the above functions.
  template <bool capture_raw>
  uc32 ScanUnicodeEscape();

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
    return token.literal_chars.is_used() &&
           (token.literal_chars.length() != source_length);
  }

#ifdef DEBUG
  void SanityCheckTokenDesc(const TokenDesc&) const;
#endif

  UnicodeCache* const unicode_cache_;

  TokenDesc& next() { return *next_; }

  const TokenDesc& current() const { return *current_; }
  const TokenDesc& next() const { return *next_; }
  const TokenDesc& next_next() const { return *next_next_; }

  TokenDesc* current_;    // desc for current token (as returned by Next())
  TokenDesc* next_;       // desc for next token (one token look-ahead)
  TokenDesc* next_next_;  // desc for the token after next (after PeakAhead())

  // Input stream. Must be initialized to an Utf16CharacterStream.
  Utf16CharacterStream* const source_;

  // One Unicode character look-ahead; c0_ < 0 at the end of the input.
  uc32 c0_;

  TokenDesc token_storage_[3];

  // Whether this scanner encountered an HTML comment.
  bool found_html_comment_;

  // Harmony flags to allow ESNext features.
  bool allow_harmony_private_fields_;
  bool allow_harmony_numeric_separator_;

  const bool is_module_;

  // Values parsed from magic comments.
  LiteralBuffer source_url_;
  LiteralBuffer source_mapping_url_;

  // Last-seen positions of potentially problematic tokens.
  Location octal_pos_;
  MessageTemplate::Template octal_message_;

  MessageTemplate::Template scanner_error_;
  Location scanner_error_location_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_SCANNER_H_
