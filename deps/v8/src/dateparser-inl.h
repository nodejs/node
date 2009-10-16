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

#ifndef V8_DATEPARSER_INL_H_
#define V8_DATEPARSER_INL_H_

#include "dateparser.h"

namespace v8 {
namespace internal {

template <typename Char>
bool DateParser::Parse(Vector<Char> str, FixedArray* out) {
  ASSERT(out->length() >= OUTPUT_SIZE);
  InputReader<Char> in(str);
  TimeZoneComposer tz;
  TimeComposer time;
  DayComposer day;

  while (!in.IsEnd()) {
    if (in.IsAsciiDigit()) {
      // Parse a number (possibly with 1 or 2 trailing colons).
      int n = in.ReadUnsignedNumber();
      if (in.Skip(':')) {
        if (in.Skip(':')) {
          // n + "::"
          if (!time.IsEmpty()) return false;
          time.Add(n);
          time.Add(0);
        } else {
          // n + ":"
          if (!time.Add(n)) return false;
        }
      } else if (tz.IsExpecting(n)) {
        tz.SetAbsoluteMinute(n);
      } else if (time.IsExpecting(n)) {
        time.AddFinal(n);
        // Require end or white space immediately after finalizing time.
        if (!in.IsEnd() && !in.SkipWhiteSpace()) return false;
      } else {
        if (!day.Add(n)) return false;
        in.Skip('-');  // Ignore suffix '-' for year, month, or day.
      }
    } else if (in.IsAsciiAlphaOrAbove()) {
      // Parse a "word" (sequence of chars. >= 'A').
      uint32_t pre[KeywordTable::kPrefixLength];
      int len = in.ReadWord(pre, KeywordTable::kPrefixLength);
      int index = KeywordTable::Lookup(pre, len);
      KeywordType type = KeywordTable::GetType(index);

      if (type == AM_PM && !time.IsEmpty()) {
        time.SetHourOffset(KeywordTable::GetValue(index));
      } else if (type == MONTH_NAME) {
        day.SetNamedMonth(KeywordTable::GetValue(index));
        in.Skip('-');  // Ignore suffix '-' for month names
      } else if (type == TIME_ZONE_NAME && in.HasReadNumber()) {
        tz.Set(KeywordTable::GetValue(index));
      } else {
        // Garbage words are illegal if a number has been read.
        if (in.HasReadNumber()) return false;
      }
    } else if (in.IsAsciiSign() && (tz.IsUTC() || !time.IsEmpty())) {
      // Parse UTC offset (only after UTC or time).
      tz.SetSign(in.GetAsciiSignValue());
      in.Next();
      int n = in.ReadUnsignedNumber();
      if (in.Skip(':')) {
        tz.SetAbsoluteHour(n);
        tz.SetAbsoluteMinute(kNone);
      } else {
        tz.SetAbsoluteHour(n / 100);
        tz.SetAbsoluteMinute(n % 100);
      }
    } else if (in.Is('(')) {
      // Ignore anything from '(' to a matching ')' or end of string.
      in.SkipParentheses();
    } else if ((in.IsAsciiSign() || in.Is(')')) && in.HasReadNumber()) {
      // Extra sign or ')' is illegal if a number has been read.
      return false;
    } else {
      // Ignore other characters.
      in.Next();
    }
  }
  return day.Write(out) && time.Write(out) && tz.Write(out);
}

} }  // namespace v8::internal

#endif  // V8_DATEPARSER_INL_H_
