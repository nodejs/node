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

#include "v8.h"

#include "dateparser.h"

namespace v8 {
namespace internal {

bool DateParser::DayComposer::Write(FixedArray* output) {
  if (index_ < 1) return false;
  // Day and month defaults to 1.
  while (index_ < kSize) {
    comp_[index_++] = 1;
  }

  int year = 0;  // Default year is 0 (=> 2000) for KJS compatibility.
  int month = kNone;
  int day = kNone;

  if (named_month_ == kNone) {
    if (is_iso_date_ || (index_ == 3 && !IsDay(comp_[0]))) {
      // YMD
      year = comp_[0];
      month = comp_[1];
      day = comp_[2];
    } else {
      // MD(Y)
      month = comp_[0];
      day = comp_[1];
      if (index_ == 3) year = comp_[2];
    }
  } else {
    month = named_month_;
    if (index_ == 1) {
      // MD or DM
      day = comp_[0];
    } else if (!IsDay(comp_[0])) {
      // YMD, MYD, or YDM
      year = comp_[0];
      day = comp_[1];
    } else {
      // DMY, MDY, or DYM
      day = comp_[0];
      year = comp_[1];
    }
  }

  if (!is_iso_date_) {
    if (Between(year, 0, 49)) year += 2000;
    else if (Between(year, 50, 99)) year += 1900;
  }

  if (!Smi::IsValid(year) || !IsMonth(month) || !IsDay(day)) return false;

  output->set(YEAR, Smi::FromInt(year));
  output->set(MONTH, Smi::FromInt(month - 1));  // 0-based
  output->set(DAY, Smi::FromInt(day));
  return true;
}


bool DateParser::TimeComposer::Write(FixedArray* output) {
  // All time slots default to 0
  while (index_ < kSize) {
    comp_[index_++] = 0;
  }

  int& hour = comp_[0];
  int& minute = comp_[1];
  int& second = comp_[2];
  int& millisecond = comp_[3];

  if (hour_offset_ != kNone) {
    if (!IsHour12(hour)) return false;
    hour %= 12;
    hour += hour_offset_;
  }

  if (!IsHour(hour) || !IsMinute(minute) ||
      !IsSecond(second) || !IsMillisecond(millisecond)) return false;

  output->set(HOUR, Smi::FromInt(hour));
  output->set(MINUTE, Smi::FromInt(minute));
  output->set(SECOND, Smi::FromInt(second));
  output->set(MILLISECOND, Smi::FromInt(millisecond));
  return true;
}


bool DateParser::TimeZoneComposer::Write(FixedArray* output) {
  if (sign_ != kNone) {
    if (hour_ == kNone) hour_ = 0;
    if (minute_ == kNone) minute_ = 0;
    int total_seconds = sign_ * (hour_ * 3600 + minute_ * 60);
    if (!Smi::IsValid(total_seconds)) return false;
    output->set(UTC_OFFSET, Smi::FromInt(total_seconds));
  } else {
    output->set_null(UTC_OFFSET);
  }
  return true;
}

const int8_t DateParser::KeywordTable::
    array[][DateParser::KeywordTable::kEntrySize] = {
  {'j', 'a', 'n', DateParser::MONTH_NAME, 1},
  {'f', 'e', 'b', DateParser::MONTH_NAME, 2},
  {'m', 'a', 'r', DateParser::MONTH_NAME, 3},
  {'a', 'p', 'r', DateParser::MONTH_NAME, 4},
  {'m', 'a', 'y', DateParser::MONTH_NAME, 5},
  {'j', 'u', 'n', DateParser::MONTH_NAME, 6},
  {'j', 'u', 'l', DateParser::MONTH_NAME, 7},
  {'a', 'u', 'g', DateParser::MONTH_NAME, 8},
  {'s', 'e', 'p', DateParser::MONTH_NAME, 9},
  {'o', 'c', 't', DateParser::MONTH_NAME, 10},
  {'n', 'o', 'v', DateParser::MONTH_NAME, 11},
  {'d', 'e', 'c', DateParser::MONTH_NAME, 12},
  {'a', 'm', '\0', DateParser::AM_PM, 0},
  {'p', 'm', '\0', DateParser::AM_PM, 12},
  {'u', 't', '\0', DateParser::TIME_ZONE_NAME, 0},
  {'u', 't', 'c', DateParser::TIME_ZONE_NAME, 0},
  {'z', '\0', '\0', DateParser::TIME_ZONE_NAME, 0},
  {'g', 'm', 't', DateParser::TIME_ZONE_NAME, 0},
  {'c', 'd', 't', DateParser::TIME_ZONE_NAME, -5},
  {'c', 's', 't', DateParser::TIME_ZONE_NAME, -6},
  {'e', 'd', 't', DateParser::TIME_ZONE_NAME, -4},
  {'e', 's', 't', DateParser::TIME_ZONE_NAME, -5},
  {'m', 'd', 't', DateParser::TIME_ZONE_NAME, -6},
  {'m', 's', 't', DateParser::TIME_ZONE_NAME, -7},
  {'p', 'd', 't', DateParser::TIME_ZONE_NAME, -7},
  {'p', 's', 't', DateParser::TIME_ZONE_NAME, -8},
  {'t', '\0', '\0', DateParser::TIME_SEPARATOR, 0},
  {'\0', '\0', '\0', DateParser::INVALID, 0},
};


// We could use perfect hashing here, but this is not a bottleneck.
int DateParser::KeywordTable::Lookup(const uint32_t* pre, int len) {
  int i;
  for (i = 0; array[i][kTypeOffset] != INVALID; i++) {
    int j = 0;
    while (j < kPrefixLength &&
           pre[j] == static_cast<uint32_t>(array[i][j])) {
      j++;
    }
    // Check if we have a match and the length is legal.
    // Word longer than keyword is only allowed for month names.
    if (j == kPrefixLength &&
        (len <= kPrefixLength || array[i][kTypeOffset] == MONTH_NAME)) {
      return i;
    }
  }
  return i;
}


int DateParser::ReadMilliseconds(DateToken token) {
  // Read first three significant digits of the original numeral,
  // as inferred from the value and the number of digits.
  // I.e., use the number of digits to see if there were
  // leading zeros.
  int number = token.number();
  int length = token.length();
  if (length < 3) {
    // Less than three digits. Multiply to put most significant digit
    // in hundreds position.
    if (length == 1) {
      number *= 100;
    } else if (length == 2) {
      number *= 10;
    }
  } else if (length > 3) {
    if (length > kMaxSignificantDigits) length = kMaxSignificantDigits;
    // More than three digits. Divide by 10^(length - 3) to get three
    // most significant digits.
    int factor = 1;
    do {
      ASSERT(factor <= 100000000);  // factor won't overflow.
      factor *= 10;
      length--;
    } while (length > 3);
    number /= factor;
  }
  return number;
}


} }  // namespace v8::internal
