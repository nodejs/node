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

#ifndef V8_EXTENSIONS_EXPERIMENTAL_LANGUAGE_MATCHER_H_
#define V8_EXTENSIONS_EXPERIMENTAL_LANGUAGE_MATCHER_H_

#include "include/v8.h"

#include "unicode/uloc.h"

namespace v8 {
namespace internal {

struct LocaleIDMatch {
  LocaleIDMatch();

  LocaleIDMatch& operator=(const LocaleIDMatch& rhs);

  // Bcp47 locale id - "de-Latn-DE-u-co-phonebk".
  char bcp47_id[ULOC_FULLNAME_CAPACITY];

  // ICU locale id - "de_Latn_DE@collation=phonebk".
  char icu_id[ULOC_FULLNAME_CAPACITY];

  // Score for this locale.
  int score;
};

class LanguageMatcher {
 public:
  // Default locale.
  static const char* const kDefaultLocale;

  // Finds best supported locale for a given a list of locale identifiers.
  // It preserves the extension for the locale id.
  static void GetBestMatchForPriorityList(
      v8::Handle<v8::Array> locale_list, LocaleIDMatch* result);

  // Finds best supported locale for a single locale identifier.
  // It preserves the extension for the locale id.
  static void GetBestMatchForString(
      v8::Handle<v8::String> locale_id, LocaleIDMatch* result);

 private:
  // If langauge subtags match add this amount to the score.
  static const unsigned int kLanguageWeight;

  // If script subtags match add this amount to the score.
  static const unsigned int kScriptWeight;

  // If region subtags match add this amount to the score.
  static const unsigned int kRegionWeight;

  // LocaleID match score has to be over this number to accept the match.
  static const unsigned int kThreshold;

  // For breaking ties in priority queue.
  static const unsigned int kPositionBonus;

  LanguageMatcher();

  // Compares locale_id to the supported list of locales and returns best
  // match.
  // Returns false if it fails to convert locale id from ICU to BCP47 format.
  static bool CompareToSupportedLocaleIDList(v8::Handle<v8::String> locale_id,
                                             LocaleIDMatch* result);
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_EXPERIMENTAL_LANGUAGE_MATCHER_H_
