// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DATE_DATEPARSER_INL_H_
#define V8_DATE_DATEPARSER_INL_H_

#include "src/date/dateparser.h"
#include "src/execution/isolate.h"
#include "src/strings/char-predicates-inl.h"

namespace v8 {
namespace internal {

template <typename Char>
bool DateParser::Parse(Isolate* isolate, Vector<Char> str, FixedArray out) {
  DCHECK(out.length() >= OUTPUT_SIZE);
  InputReader<Char> in(str);
  DateStringTokenizer<Char> scanner(&in);
  TimeZoneComposer tz;
  TimeComposer time;
  DayComposer day;

  // Specification:
  // Accept ES5 ISO 8601 date-time-strings or legacy dates compatible
  // with Safari.
  // ES5 ISO 8601 dates:
  //   [('-'|'+')yy]yyyy[-MM[-DD]][THH:mm[:ss[.sss]][Z|(+|-)hh:mm]]
  //   where yyyy is in the range 0000..9999 and
  //         +/-yyyyyy is in the range -999999..+999999 -
  //           but -000000 is invalid (year zero must be positive),
  //         MM is in the range 01..12,
  //         DD is in the range 01..31,
  //         MM and DD defaults to 01 if missing,,
  //         HH is generally in the range 00..23, but can be 24 if mm, ss
  //           and sss are zero (or missing), representing midnight at the
  //           end of a day,
  //         mm and ss are in the range 00..59,
  //         sss is in the range 000..999,
  //         hh is in the range 00..23,
  //         mm, ss, and sss default to 00 if missing, and
  //         timezone defaults to Z if missing
  //           (following Safari, ISO actually demands local time).
  //  Extensions:
  //   We also allow sss to have more or less than three digits (but at
  //   least one).
  //   We allow hh:mm to be specified as hhmm.
  // Legacy dates:
  //  Any unrecognized word before the first number is ignored.
  //  Parenthesized text is ignored.
  //  An unsigned number followed by ':' is a time value, and is
  //  added to the TimeComposer. A number followed by '::' adds a second
  //  zero as well. A number followed by '.' is also a time and must be
  //  followed by milliseconds.
  //  Any other number is a date component and is added to DayComposer.
  //  A month name (or really: any word having the same first three letters
  //  as a month name) is recorded as a named month in the Day composer.
  //  A word recognizable as a time-zone is recorded as such, as is
  //  '(+|-)(hhmm|hh:)'.
  //  Legacy dates don't allow extra signs ('+' or '-') or umatched ')'
  //  after a number has been read (before the first number, any garbage
  //  is allowed).
  // Intersection of the two:
  //  A string that matches both formats (e.g. 1970-01-01) will be
  //  parsed as an ES5 date-time string - which means it will default
  //  to UTC time-zone. That's unavoidable if following the ES5
  //  specification.
  //  After a valid "T" has been read while scanning an ES5 datetime string,
  //  the input can no longer be a valid legacy date, since the "T" is a
  //  garbage string after a number has been read.

  // First try getting as far as possible with as ES5 Date Time String.
  DateToken next_unhandled_token = ParseES5DateTime(&scanner, &day, &time, &tz);
  if (next_unhandled_token.IsInvalid()) return false;
  bool has_read_number = !day.IsEmpty();
  // If there's anything left, continue with the legacy parser.
  bool legacy_parser = false;
  for (DateToken token = next_unhandled_token; !token.IsEndOfInput();
       token = scanner.Next()) {
    if (token.IsNumber()) {
      legacy_parser = true;
      has_read_number = true;
      int n = token.number();
      if (scanner.SkipSymbol(':')) {
        if (scanner.SkipSymbol(':')) {
          // n + "::"
          if (!time.IsEmpty()) return false;
          time.Add(n);
          time.Add(0);
        } else {
          // n + ":"
          if (!time.Add(n)) return false;
          if (scanner.Peek().IsSymbol('.')) scanner.Next();
        }
      } else if (scanner.SkipSymbol('.') && time.IsExpecting(n)) {
        time.Add(n);
        if (!scanner.Peek().IsNumber()) return false;
        int n = ReadMilliseconds(scanner.Next());
        if (n < 0) return false;
        time.AddFinal(n);
      } else if (tz.IsExpecting(n)) {
        tz.SetAbsoluteMinute(n);
      } else if (time.IsExpecting(n)) {
        time.AddFinal(n);
        // Require end, white space, "Z", "+" or "-" immediately after
        // finalizing time.
        DateToken peek = scanner.Peek();
        if (!peek.IsEndOfInput() && !peek.IsWhiteSpace() &&
            !peek.IsKeywordZ() && !peek.IsAsciiSign())
          return false;
      } else {
        if (!day.Add(n)) return false;
        scanner.SkipSymbol('-');
      }
    } else if (token.IsKeyword()) {
      legacy_parser = true;
      // Parse a "word" (sequence of chars. >= 'A').
      KeywordType type = token.keyword_type();
      int value = token.keyword_value();
      if (type == AM_PM && !time.IsEmpty()) {
        time.SetHourOffset(value);
      } else if (type == MONTH_NAME) {
        day.SetNamedMonth(value);
        scanner.SkipSymbol('-');
      } else if (type == TIME_ZONE_NAME && has_read_number) {
        tz.Set(value);
      } else {
        // Garbage words are illegal if a number has been read.
        if (has_read_number) return false;
        // The first number has to be separated from garbage words by
        // whitespace or other separators.
        if (scanner.Peek().IsNumber()) return false;
      }
    } else if (token.IsAsciiSign() && (tz.IsUTC() || !time.IsEmpty())) {
      legacy_parser = true;
      // Parse UTC offset (only after UTC or time).
      tz.SetSign(token.ascii_sign());
      // The following number may be empty.
      int n = 0;
      int length = 0;
      if (scanner.Peek().IsNumber()) {
        DateToken token = scanner.Next();
        length = token.length();
        n = token.number();
      }
      has_read_number = true;

      if (scanner.Peek().IsSymbol(':')) {
        tz.SetAbsoluteHour(n);
        // TODO(littledan): Use minutes as part of timezone?
        tz.SetAbsoluteMinute(kNone);
      } else if (length == 2 || length == 1) {
        // Handle time zones like GMT-8
        tz.SetAbsoluteHour(n);
        tz.SetAbsoluteMinute(0);
      } else if (length == 4 || length == 3) {
        // Looks like the hhmm format
        tz.SetAbsoluteHour(n / 100);
        tz.SetAbsoluteMinute(n % 100);
      } else {
        // No need to accept time zones like GMT-12345
        return false;
      }
    } else if ((token.IsAsciiSign() || token.IsSymbol(')')) &&
               has_read_number) {
      // Extra sign or ')' is illegal if a number has been read.
      return false;
    } else {
      // Ignore other characters and whitespace.
    }
  }

  bool success = day.Write(out) && time.Write(out) && tz.Write(out);

  if (legacy_parser && success) {
    isolate->CountUsage(v8::Isolate::kLegacyDateParser);
  }

  return success;
}

template <typename CharType>
DateParser::DateToken DateParser::DateStringTokenizer<CharType>::Scan() {
  int pre_pos = in_->position();
  if (in_->IsEnd()) return DateToken::EndOfInput();
  if (in_->IsAsciiDigit()) {
    int n = in_->ReadUnsignedNumeral();
    int length = in_->position() - pre_pos;
    return DateToken::Number(n, length);
  }
  if (in_->Skip(':')) return DateToken::Symbol(':');
  if (in_->Skip('-')) return DateToken::Symbol('-');
  if (in_->Skip('+')) return DateToken::Symbol('+');
  if (in_->Skip('.')) return DateToken::Symbol('.');
  if (in_->Skip(')')) return DateToken::Symbol(')');
  if (in_->IsAsciiAlphaOrAbove()) {
    DCHECK_EQ(KeywordTable::kPrefixLength, 3);
    uint32_t buffer[3] = {0, 0, 0};
    int length = in_->ReadWord(buffer, 3);
    int index = KeywordTable::Lookup(buffer, length);
    return DateToken::Keyword(KeywordTable::GetType(index),
                              KeywordTable::GetValue(index), length);
  }
  if (in_->SkipWhiteSpace()) {
    return DateToken::WhiteSpace(in_->position() - pre_pos);
  }
  if (in_->SkipParentheses()) {
    return DateToken::Unknown();
  }
  in_->Next();
  return DateToken::Unknown();
}

template <typename Char>
bool DateParser::InputReader<Char>::SkipWhiteSpace() {
  if (IsWhiteSpaceOrLineTerminator(ch_)) {
    Next();
    return true;
  }
  return false;
}

template <typename Char>
bool DateParser::InputReader<Char>::SkipParentheses() {
  if (ch_ != '(') return false;
  int balance = 0;
  do {
    if (ch_ == ')')
      --balance;
    else if (ch_ == '(')
      ++balance;
    Next();
  } while (balance > 0 && ch_);
  return true;
}

template <typename Char>
DateParser::DateToken DateParser::ParseES5DateTime(
    DateStringTokenizer<Char>* scanner, DayComposer* day, TimeComposer* time,
    TimeZoneComposer* tz) {
  DCHECK(day->IsEmpty());
  DCHECK(time->IsEmpty());
  DCHECK(tz->IsEmpty());

  // Parse mandatory date string: [('-'|'+')yy]yyyy[':'MM[':'DD]]
  if (scanner->Peek().IsAsciiSign()) {
    // Keep the sign token, so we can pass it back to the legacy
    // parser if we don't use it.
    DateToken sign_token = scanner->Next();
    if (!scanner->Peek().IsFixedLengthNumber(6)) return sign_token;
    int sign = sign_token.ascii_sign();
    int year = scanner->Next().number();
    if (sign < 0 && year == 0) return sign_token;
    day->Add(sign * year);
  } else if (scanner->Peek().IsFixedLengthNumber(4)) {
    day->Add(scanner->Next().number());
  } else {
    return scanner->Next();
  }
  if (scanner->SkipSymbol('-')) {
    if (!scanner->Peek().IsFixedLengthNumber(2) ||
        !DayComposer::IsMonth(scanner->Peek().number()))
      return scanner->Next();
    day->Add(scanner->Next().number());
    if (scanner->SkipSymbol('-')) {
      if (!scanner->Peek().IsFixedLengthNumber(2) ||
          !DayComposer::IsDay(scanner->Peek().number()))
        return scanner->Next();
      day->Add(scanner->Next().number());
    }
  }
  // Check for optional time string: 'T'HH':'mm[':'ss['.'sss]]Z
  if (!scanner->Peek().IsKeywordType(TIME_SEPARATOR)) {
    if (!scanner->Peek().IsEndOfInput()) return scanner->Next();
  } else {
    // ES5 Date Time String time part is present.
    scanner->Next();
    if (!scanner->Peek().IsFixedLengthNumber(2) ||
        !Between(scanner->Peek().number(), 0, 24)) {
      return DateToken::Invalid();
    }
    // Allow 24:00[:00[.000]], but no other time starting with 24.
    bool hour_is_24 = (scanner->Peek().number() == 24);
    time->Add(scanner->Next().number());
    if (!scanner->SkipSymbol(':')) return DateToken::Invalid();
    if (!scanner->Peek().IsFixedLengthNumber(2) ||
        !TimeComposer::IsMinute(scanner->Peek().number()) ||
        (hour_is_24 && scanner->Peek().number() > 0)) {
      return DateToken::Invalid();
    }
    time->Add(scanner->Next().number());
    if (scanner->SkipSymbol(':')) {
      if (!scanner->Peek().IsFixedLengthNumber(2) ||
          !TimeComposer::IsSecond(scanner->Peek().number()) ||
          (hour_is_24 && scanner->Peek().number() > 0)) {
        return DateToken::Invalid();
      }
      time->Add(scanner->Next().number());
      if (scanner->SkipSymbol('.')) {
        if (!scanner->Peek().IsNumber() ||
            (hour_is_24 && scanner->Peek().number() > 0)) {
          return DateToken::Invalid();
        }
        // Allow more or less than the mandated three digits.
        time->Add(ReadMilliseconds(scanner->Next()));
      }
    }
    // Check for optional timezone designation: 'Z' | ('+'|'-')hh':'mm
    if (scanner->Peek().IsKeywordZ()) {
      scanner->Next();
      tz->Set(0);
    } else if (scanner->Peek().IsSymbol('+') || scanner->Peek().IsSymbol('-')) {
      tz->SetSign(scanner->Next().symbol() == '+' ? 1 : -1);
      if (scanner->Peek().IsFixedLengthNumber(4)) {
        // hhmm extension syntax.
        int hourmin = scanner->Next().number();
        int hour = hourmin / 100;
        int min = hourmin % 100;
        if (!TimeComposer::IsHour(hour) || !TimeComposer::IsMinute(min)) {
          return DateToken::Invalid();
        }
        tz->SetAbsoluteHour(hour);
        tz->SetAbsoluteMinute(min);
      } else {
        // hh:mm standard syntax.
        if (!scanner->Peek().IsFixedLengthNumber(2) ||
            !TimeComposer::IsHour(scanner->Peek().number())) {
          return DateToken::Invalid();
        }
        tz->SetAbsoluteHour(scanner->Next().number());
        if (!scanner->SkipSymbol(':')) return DateToken::Invalid();
        if (!scanner->Peek().IsFixedLengthNumber(2) ||
            !TimeComposer::IsMinute(scanner->Peek().number())) {
          return DateToken::Invalid();
        }
        tz->SetAbsoluteMinute(scanner->Next().number());
      }
    }
    if (!scanner->Peek().IsEndOfInput()) return DateToken::Invalid();
  }
  // Successfully parsed ES5 Date Time String.
  // ES#sec-date-time-string-format Date Time String Format
  // "When the time zone offset is absent, date-only forms are interpreted
  //  as a UTC time and date-time forms are interpreted as a local time."
  if (tz->IsEmpty() && time->IsEmpty()) {
    tz->Set(0);
  }
  day->set_iso_date();
  return DateToken::EndOfInput();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_DATE_DATEPARSER_INL_H_
