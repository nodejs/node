// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DATE_DATEPARSER_H_
#define V8_DATE_DATEPARSER_H_

#include "src/strings/char-predicates.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class DateParser : public AllStatic {
 public:
  enum {
    YEAR,
    MONTH,
    DAY,
    HOUR,
    MINUTE,
    SECOND,
    MILLISECOND,
    UTC_OFFSET,
    OUTPUT_SIZE
  };

  // Parse the string as a date. If parsing succeeds, return true after
  // filling out the output array as follows (all integers are Smis):
  // [0]: year
  // [1]: month (0 = Jan, 1 = Feb, ...)
  // [2]: day
  // [3]: hour
  // [4]: minute
  // [5]: second
  // [6]: millisecond
  // [7]: UTC offset in seconds, or null value if no timezone specified
  // If parsing fails, return false (content of output array is not defined).
  template <typename Char>
  static bool Parse(Isolate* isolate, Vector<Char> str, double* output);

 private:
  // Range testing
  static inline bool Between(int x, int lo, int hi) {
    return static_cast<unsigned>(x - lo) <= static_cast<unsigned>(hi - lo);
  }

  // Indicates a missing value.
  static const int kNone = kMaxInt;

  // Maximal number of digits used to build the value of a numeral.
  // Remaining digits are ignored.
  static const int kMaxSignificantDigits = 9;

  // InputReader provides basic string parsing and character classification.
  template <typename Char>
  class InputReader {
   public:
    explicit InputReader(Vector<Char> s) : index_(0), buffer_(s) { Next(); }

    int position() { return index_; }

    // Advance to the next character of the string.
    void Next() {
      ch_ = (index_ < buffer_.length()) ? buffer_[index_] : 0;
      index_++;
    }

    // Read a string of digits as an unsigned number. Cap value at
    // kMaxSignificantDigits, but skip remaining digits if the numeral
    // is longer.
    int ReadUnsignedNumeral() {
      int n = 0;
      int i = 0;
      while (IsAsciiDigit()) {
        if (i < kMaxSignificantDigits) n = n * 10 + ch_ - '0';
        i++;
        Next();
      }
      return n;
    }

    // Read a word (sequence of chars. >= 'A'), fill the given buffer with a
    // lower-case prefix, and pad any remainder of the buffer with zeroes.
    // Return word length.
    int ReadWord(uint32_t* prefix, int prefix_size) {
      int len;
      for (len = 0; IsAsciiAlphaOrAbove(); Next(), len++) {
        if (len < prefix_size) prefix[len] = AsciiAlphaToLower(ch_);
      }
      for (int i = len; i < prefix_size; i++) prefix[i] = 0;
      return len;
    }

    // The skip methods return whether they actually skipped something.
    bool Skip(uint32_t c) {
      if (ch_ == c) {
        Next();
        return true;
      }
      return false;
    }

    inline bool SkipWhiteSpace();
    inline bool SkipParentheses();

    // Character testing/classification. Non-ASCII digits are not supported.
    bool Is(uint32_t c) const { return ch_ == c; }
    bool IsEnd() const { return ch_ == 0; }
    bool IsAsciiDigit() const { return IsDecimalDigit(ch_); }
    bool IsAsciiAlphaOrAbove() const { return ch_ >= 'A'; }
    bool IsAsciiSign() const { return ch_ == '+' || ch_ == '-'; }

    // Return 1 for '+' and -1 for '-'.
    int GetAsciiSignValue() const { return 44 - static_cast<int>(ch_); }

   private:
    int index_;
    Vector<Char> buffer_;
    uint32_t ch_;
  };

  enum KeywordType {
    INVALID,
    MONTH_NAME,
    TIME_ZONE_NAME,
    TIME_SEPARATOR,
    AM_PM
  };

  struct DateToken {
   public:
    bool IsInvalid() { return tag_ == kInvalidTokenTag; }
    bool IsUnknown() { return tag_ == kUnknownTokenTag; }
    bool IsNumber() { return tag_ == kNumberTag; }
    bool IsSymbol() { return tag_ == kSymbolTag; }
    bool IsWhiteSpace() { return tag_ == kWhiteSpaceTag; }
    bool IsEndOfInput() { return tag_ == kEndOfInputTag; }
    bool IsKeyword() { return tag_ >= kKeywordTagStart; }

    int length() { return length_; }

    int number() {
      DCHECK(IsNumber());
      return value_;
    }
    KeywordType keyword_type() {
      DCHECK(IsKeyword());
      return static_cast<KeywordType>(tag_);
    }
    int keyword_value() {
      DCHECK(IsKeyword());
      return value_;
    }
    char symbol() {
      DCHECK(IsSymbol());
      return static_cast<char>(value_);
    }
    bool IsSymbol(char symbol) {
      return IsSymbol() && this->symbol() == symbol;
    }
    bool IsKeywordType(KeywordType tag) { return tag_ == tag; }
    bool IsFixedLengthNumber(int length) {
      return IsNumber() && length_ == length;
    }
    bool IsAsciiSign() {
      return tag_ == kSymbolTag && (value_ == '-' || value_ == '+');
    }
    int ascii_sign() {
      DCHECK(IsAsciiSign());
      return 44 - value_;
    }
    bool IsKeywordZ() {
      return IsKeywordType(TIME_ZONE_NAME) && length_ == 1 && value_ == 0;
    }
    bool IsUnknown(int character) { return IsUnknown() && value_ == character; }
    // Factory functions.
    static DateToken Keyword(KeywordType tag, int value, int length) {
      return DateToken(tag, length, value);
    }
    static DateToken Number(int value, int length) {
      return DateToken(kNumberTag, length, value);
    }
    static DateToken Symbol(char symbol) {
      return DateToken(kSymbolTag, 1, symbol);
    }
    static DateToken EndOfInput() { return DateToken(kEndOfInputTag, 0, -1); }
    static DateToken WhiteSpace(int length) {
      return DateToken(kWhiteSpaceTag, length, -1);
    }
    static DateToken Unknown() { return DateToken(kUnknownTokenTag, 1, -1); }
    static DateToken Invalid() { return DateToken(kInvalidTokenTag, 0, -1); }

   private:
    enum TagType {
      kInvalidTokenTag = -6,
      kUnknownTokenTag = -5,
      kWhiteSpaceTag = -4,
      kNumberTag = -3,
      kSymbolTag = -2,
      kEndOfInputTag = -1,
      kKeywordTagStart = 0
    };
    DateToken(int tag, int length, int value)
        : tag_(tag), length_(length), value_(value) {}

    int tag_;
    int length_;  // Number of characters.
    int value_;
  };

  template <typename Char>
  class DateStringTokenizer {
   public:
    explicit DateStringTokenizer(InputReader<Char>* in)
        : in_(in), next_(Scan()) {}
    DateToken Next() {
      DateToken result = next_;
      next_ = Scan();
      return result;
    }

    DateToken Peek() { return next_; }
    bool SkipSymbol(char symbol) {
      if (next_.IsSymbol(symbol)) {
        next_ = Scan();
        return true;
      }
      return false;
    }

   private:
    DateToken Scan();

    InputReader<Char>* in_;
    DateToken next_;
  };

  static int ReadMilliseconds(DateToken number);

  // KeywordTable maps names of months, time zones, am/pm to numbers.
  class KeywordTable : public AllStatic {
   public:
    // Look up a word in the keyword table and return an index.
    // 'pre' contains a prefix of the word, zero-padded to size kPrefixLength
    // and 'len' is the word length.
    static int Lookup(const uint32_t* pre, int len);
    // Get the type of the keyword at index i.
    static KeywordType GetType(int i) {
      return static_cast<KeywordType>(array[i][kTypeOffset]);
    }
    // Get the value of the keyword at index i.
    static int GetValue(int i) { return array[i][kValueOffset]; }

    static const int kPrefixLength = 3;
    static const int kTypeOffset = kPrefixLength;
    static const int kValueOffset = kTypeOffset + 1;
    static const int kEntrySize = kValueOffset + 1;
    static const int8_t array[][kEntrySize];
  };

  class TimeZoneComposer {
   public:
    TimeZoneComposer() : sign_(kNone), hour_(kNone), minute_(kNone) {}
    void Set(int offset_in_hours) {
      sign_ = offset_in_hours < 0 ? -1 : 1;
      hour_ = offset_in_hours * sign_;
      minute_ = 0;
    }
    void SetSign(int sign) { sign_ = sign < 0 ? -1 : 1; }
    void SetAbsoluteHour(int hour) { hour_ = hour; }
    void SetAbsoluteMinute(int minute) { minute_ = minute; }
    bool IsExpecting(int n) const {
      return hour_ != kNone && minute_ == kNone && TimeComposer::IsMinute(n);
    }
    bool IsUTC() const { return hour_ == 0 && minute_ == 0; }
    bool Write(double* output);
    bool IsEmpty() { return hour_ == kNone; }

   private:
    int sign_;
    int hour_;
    int minute_;
  };

  class TimeComposer {
   public:
    TimeComposer() : index_(0), hour_offset_(kNone) {}
    bool IsEmpty() const { return index_ == 0; }
    bool IsExpecting(int n) const {
      return (index_ == 1 && IsMinute(n)) || (index_ == 2 && IsSecond(n)) ||
             (index_ == 3 && IsMillisecond(n));
    }
    bool Add(int n) {
      return index_ < kSize ? (comp_[index_++] = n, true) : false;
    }
    bool AddFinal(int n) {
      if (!Add(n)) return false;
      while (index_ < kSize) comp_[index_++] = 0;
      return true;
    }
    void SetHourOffset(int n) { hour_offset_ = n; }
    bool Write(double* output);

    static bool IsMinute(int x) { return Between(x, 0, 59); }
    static bool IsHour(int x) { return Between(x, 0, 23); }
    static bool IsSecond(int x) { return Between(x, 0, 59); }

   private:
    static bool IsHour12(int x) { return Between(x, 0, 12); }
    static bool IsMillisecond(int x) { return Between(x, 0, 999); }

    static const int kSize = 4;
    int comp_[kSize];
    int index_;
    int hour_offset_;
  };

  class DayComposer {
   public:
    DayComposer() : index_(0), named_month_(kNone), is_iso_date_(false) {}
    bool IsEmpty() const { return index_ == 0; }
    bool Add(int n) {
      if (index_ < kSize) {
        comp_[index_] = n;
        index_++;
        return true;
      }
      return false;
    }
    void SetNamedMonth(int n) { named_month_ = n; }
    bool Write(double* output);
    void set_iso_date() { is_iso_date_ = true; }
    static bool IsMonth(int x) { return Between(x, 1, 12); }
    static bool IsDay(int x) { return Between(x, 1, 31); }

   private:
    static const int kSize = 3;
    int comp_[kSize];
    int index_;
    int named_month_;
    // If set, ensures that data is always parsed in year-month-date order.
    bool is_iso_date_;
  };

  // Tries to parse an ES5 Date Time String. Returns the next token
  // to continue with in the legacy date string parser. If parsing is
  // complete, returns DateToken::EndOfInput(). If terminally unsuccessful,
  // returns DateToken::Invalid(). Otherwise parsing continues in the
  // legacy parser.
  template <typename Char>
  static DateParser::DateToken ParseES5DateTime(
      DateStringTokenizer<Char>* scanner, DayComposer* day, TimeComposer* time,
      TimeZoneComposer* tz);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DATE_DATEPARSER_H_
