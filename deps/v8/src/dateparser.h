// Copyright 2008 the V8 project authors. All rights reserved.
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

#ifndef V8_DATEPARSER_H_
#define V8_DATEPARSER_H_

#include "scanner.h"

namespace v8 {
namespace internal {

class DateParser : public AllStatic {
 public:

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
  static bool Parse(Vector<Char> str, FixedArray* output);

  enum {
    YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, MILLISECOND, UTC_OFFSET, OUTPUT_SIZE
  };

 private:
  // Range testing
  static inline bool Between(int x, int lo, int hi) {
    return static_cast<unsigned>(x - lo) <= static_cast<unsigned>(hi - lo);
  }
  // Indicates a missing value.
  static const int kNone = kMaxInt;

  // InputReader provides basic string parsing and character classification.
  template <typename Char>
  class InputReader BASE_EMBEDDED {
   public:
    explicit InputReader(Vector<Char> s)
        : index_(0),
          buffer_(s),
          has_read_number_(false) {
      Next();
    }

    // Advance to the next character of the string.
    void Next() { ch_ = (index_ < buffer_.length()) ? buffer_[index_++] : 0; }

    // Read a string of digits as an unsigned number (cap just below kMaxInt).
    int ReadUnsignedNumber() {
      has_read_number_ = true;
      int n;
      for (n = 0; IsAsciiDigit() && n < kMaxInt / 10 - 1; Next()) {
        n = n * 10 + ch_ - '0';
      }
      return n;
    }

    // Read a word (sequence of chars. >= 'A'), fill the given buffer with a
    // lower-case prefix, and pad any remainder of the buffer with zeroes.
    // Return word length.
    int ReadWord(uint32_t* prefix, int prefix_size) {
      int len;
      for (len = 0; IsAsciiAlphaOrAbove(); Next(), len++) {
        if (len < prefix_size) prefix[len] = GetAsciiAlphaLower();
      }
      for (int i = len; i < prefix_size; i++) prefix[i] = 0;
      return len;
    }

    // The skip methods return whether they actually skipped something.
    bool Skip(uint32_t c) { return ch_ == c ?  (Next(), true) : false; }

    bool SkipWhiteSpace() {
      return Scanner::kIsWhiteSpace.get(ch_) ? (Next(), true) : false;
    }

    bool SkipParentheses() {
      if (ch_ != '(') return false;
      int balance = 0;
      do {
        if (ch_ == ')') --balance;
        else if (ch_ == '(') ++balance;
        Next();
      } while (balance > 0 && ch_);
      return true;
    }

    // Character testing/classification. Non-ASCII digits are not supported.
    bool Is(uint32_t c) const { return ch_ == c; }
    bool IsEnd() const { return ch_ == 0; }
    bool IsAsciiDigit() const { return IsDecimalDigit(ch_); }
    bool IsAsciiAlphaOrAbove() const { return ch_ >= 'A'; }
    bool IsAsciiSign() const { return ch_ == '+' || ch_ == '-'; }

    // Return 1 for '+' and -1 for '-'.
    int GetAsciiSignValue() const { return 44 - static_cast<int>(ch_); }

    // Indicates whether any (possibly empty!) numbers have been read.
    bool HasReadNumber() const { return has_read_number_; }

   private:
    // If current character is in 'A'-'Z' or 'a'-'z', return its lower-case.
    // Else, return something outside of 'A'-'Z' and 'a'-'z'.
    uint32_t GetAsciiAlphaLower() const { return ch_ | 32; }

    int index_;
    Vector<Char> buffer_;
    bool has_read_number_;
    uint32_t ch_;
  };

  enum KeywordType { INVALID, MONTH_NAME, TIME_ZONE_NAME, AM_PM };

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

  class TimeZoneComposer BASE_EMBEDDED {
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
    bool Write(FixedArray* output);
   private:
    int sign_;
    int hour_;
    int minute_;
  };

  class TimeComposer BASE_EMBEDDED {
   public:
    TimeComposer() : index_(0), hour_offset_(kNone) {}
    bool IsEmpty() const { return index_ == 0; }
    bool IsExpecting(int n) const {
      return (index_ == 1 && IsMinute(n)) ||
             (index_ == 2 && IsSecond(n)) ||
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
    bool Write(FixedArray* output);

    static bool IsMinute(int x) { return Between(x, 0, 59); }
   private:
    static bool IsHour(int x) { return Between(x, 0, 23); }
    static bool IsHour12(int x) { return Between(x, 0, 12); }
    static bool IsSecond(int x) { return Between(x, 0, 59); }
    static bool IsMillisecond(int x) { return Between(x, 0, 999); }

    static const int kSize = 4;
    int comp_[kSize];
    int index_;
    int hour_offset_;
  };

  class DayComposer BASE_EMBEDDED {
   public:
    DayComposer() : index_(0), named_month_(kNone) {}
    bool IsEmpty() const { return index_ == 0; }
    bool Add(int n) {
      return index_ < kSize ? (comp_[index_++] = n, true) : false;
    }
    void SetNamedMonth(int n) { named_month_ = n; }
    bool Write(FixedArray* output);
   private:
    static bool IsMonth(int x) { return Between(x, 1, 12); }
    static bool IsDay(int x) { return Between(x, 1, 31); }

    static const int kSize = 3;
    int comp_[kSize];
    int index_;
    int named_month_;
  };
};


} }  // namespace v8::internal

#endif  // V8_DATEPARSER_H_
