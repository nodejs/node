// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

#include <google/protobuf/io/strtod.h>

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

// ----------------------------------------------------------------------
// NoLocaleStrtod()
//   This code will make you cry.
// ----------------------------------------------------------------------

namespace {

// This approximately 0x1.ffffffp127, but we don't use 0x1.ffffffp127 because
// it won't compile in MSVC.
const double MAX_FLOAT_AS_DOUBLE_ROUNDED = 3.4028235677973366e+38;

// Returns a string identical to *input except that the character pointed to
// by radix_pos (which should be '.') is replaced with the locale-specific
// radix character.
std::string LocalizeRadix(const char* input, const char* radix_pos) {
  // Determine the locale-specific radix character by calling sprintf() to
  // print the number 1.5, then stripping off the digits.  As far as I can
  // tell, this is the only portable, thread-safe way to get the C library
  // to divuldge the locale's radix character.  No, localeconv() is NOT
  // thread-safe.
  char temp[16];
  int size = sprintf(temp, "%.1f", 1.5);
  GOOGLE_CHECK_EQ(temp[0], '1');
  GOOGLE_CHECK_EQ(temp[size - 1], '5');
  GOOGLE_CHECK_LE(size, 6);

  // Now replace the '.' in the input with it.
  std::string result;
  result.reserve(strlen(input) + size - 3);
  result.append(input, radix_pos);
  result.append(temp + 1, size - 2);
  result.append(radix_pos + 1);
  return result;
}

}  // namespace

double NoLocaleStrtod(const char* text, char** original_endptr) {
  // We cannot simply set the locale to "C" temporarily with setlocale()
  // as this is not thread-safe.  Instead, we try to parse in the current
  // locale first.  If parsing stops at a '.' character, then this is a
  // pretty good hint that we're actually in some other locale in which
  // '.' is not the radix character.

  char* temp_endptr;
  double result = strtod(text, &temp_endptr);
  if (original_endptr != NULL) *original_endptr = temp_endptr;
  if (*temp_endptr != '.') return result;

  // Parsing halted on a '.'.  Perhaps we're in a different locale?  Let's
  // try to replace the '.' with a locale-specific radix character and
  // try again.
  std::string localized = LocalizeRadix(text, temp_endptr);
  const char* localized_cstr = localized.c_str();
  char* localized_endptr;
  result = strtod(localized_cstr, &localized_endptr);
  if ((localized_endptr - localized_cstr) > (temp_endptr - text)) {
    // This attempt got further, so replacing the decimal must have helped.
    // Update original_endptr to point at the right location.
    if (original_endptr != NULL) {
      // size_diff is non-zero if the localized radix has multiple bytes.
      int size_diff = localized.size() - strlen(text);
      // const_cast is necessary to match the strtod() interface.
      *original_endptr = const_cast<char*>(
          text + (localized_endptr - localized_cstr - size_diff));
    }
  }

  return result;
}

float SafeDoubleToFloat(double value) {
  // static_cast<float> on a number larger than float can result in illegal
  // instruction error, so we need to manually convert it to infinity or max.
  if (value > std::numeric_limits<float>::max()) {
    // Max float value is about 3.4028234664E38 when represented as a double.
    // However, when printing float as text, it will be rounded as
    // 3.4028235e+38. If we parse the value of 3.4028235e+38 from text and
    // compare it to 3.4028234664E38, we may think that it is larger, but
    // actually, any number between these two numbers could only be represented
    // as the same max float number in float, so we should treat them the same
    // as max float.
    if (value <= MAX_FLOAT_AS_DOUBLE_ROUNDED) {
      return std::numeric_limits<float>::max();
    }
    return std::numeric_limits<float>::infinity();
  } else if (value < -std::numeric_limits<float>::max()) {
    if (value >= -MAX_FLOAT_AS_DOUBLE_ROUNDED) {
      return -std::numeric_limits<float>::max();
    }
    return -std::numeric_limits<float>::infinity();
  } else {
    return static_cast<float>(value);
  }
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
