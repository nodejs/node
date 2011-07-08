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

// TODO(cira): Remove LanguageMatcher from v8 when ICU implements
// language matching API.

#include "src/extensions/experimental/language-matcher.h"

#include <string.h>

#include "src/extensions/experimental/i18n-utils.h"
#include "unicode/datefmt.h"  // For getAvailableLocales
#include "unicode/locid.h"
#include "unicode/uloc.h"

namespace v8 {
namespace internal {

const unsigned int LanguageMatcher::kLanguageWeight = 75;
const unsigned int LanguageMatcher::kScriptWeight = 20;
const unsigned int LanguageMatcher::kRegionWeight = 5;
const unsigned int LanguageMatcher::kThreshold = 50;
const unsigned int LanguageMatcher::kPositionBonus = 1;
const char* const LanguageMatcher::kDefaultLocale = "root";

static const char* GetLanguageException(const char*);
static bool BCP47ToICUFormat(const char*, char*);
static int CompareLocaleSubtags(const char*, const char*);
static bool BuildLocaleName(const char*, const char*, LocaleIDMatch*);

LocaleIDMatch::LocaleIDMatch()
    : score(-1) {
  I18NUtils::StrNCopy(
      bcp47_id, ULOC_FULLNAME_CAPACITY, LanguageMatcher::kDefaultLocale);

  I18NUtils::StrNCopy(
      icu_id, ULOC_FULLNAME_CAPACITY, LanguageMatcher::kDefaultLocale);
}

LocaleIDMatch& LocaleIDMatch::operator=(const LocaleIDMatch& rhs) {
  I18NUtils::StrNCopy(this->bcp47_id, ULOC_FULLNAME_CAPACITY, rhs.bcp47_id);
  I18NUtils::StrNCopy(this->icu_id, ULOC_FULLNAME_CAPACITY, rhs.icu_id);
  this->score = rhs.score;

  return *this;
}

// static
void LanguageMatcher::GetBestMatchForPriorityList(
    v8::Handle<v8::Array> locales, LocaleIDMatch* result) {
  v8::HandleScope handle_scope;

  unsigned int position_bonus = locales->Length() * kPositionBonus;

  int max_score = 0;
  LocaleIDMatch match;
  for (unsigned int i = 0; i < locales->Length(); ++i) {
    position_bonus -= kPositionBonus;

    v8::TryCatch try_catch;
    v8::Local<v8::Value> locale_id = locales->Get(v8::Integer::New(i));

    // Return default if exception is raised when reading parameter.
    if (try_catch.HasCaught()) break;

    // JavaScript arrays can be heterogenous so check each item
    // if it's a string.
    if (!locale_id->IsString()) continue;

    if (!CompareToSupportedLocaleIDList(locale_id->ToString(), &match)) {
      continue;
    }

    // Skip items under threshold.
    if (match.score < kThreshold) continue;

    match.score += position_bonus;
    if (match.score > max_score) {
      *result = match;

      max_score = match.score;
    }
  }
}

// static
void LanguageMatcher::GetBestMatchForString(
    v8::Handle<v8::String> locale, LocaleIDMatch* result) {
  LocaleIDMatch match;

  if (CompareToSupportedLocaleIDList(locale, &match) &&
      match.score >= kThreshold) {
    *result = match;
  }
}

// static
bool LanguageMatcher::CompareToSupportedLocaleIDList(
    v8::Handle<v8::String> locale_id, LocaleIDMatch* result) {
  static int32_t available_count = 0;
  // Depending on how ICU data is built, locales returned by
  // Locale::getAvailableLocale() are not guaranteed to support DateFormat,
  // Collation and other services.  We can call getAvailableLocale() of all the
  // services we want to support and take the intersection of them all, but
  // using DateFormat::getAvailableLocales() should suffice.
  // TODO(cira): Maybe make this thread-safe?
  static const icu::Locale* available_locales =
      icu::DateFormat::getAvailableLocales(available_count);

  // Skip this locale_id if it's not in ASCII.
  static LocaleIDMatch default_match;
  v8::String::AsciiValue ascii_value(locale_id);
  if (*ascii_value == NULL) return false;

  char locale[ULOC_FULLNAME_CAPACITY];
  if (!BCP47ToICUFormat(*ascii_value, locale)) return false;

  icu::Locale input_locale(locale);

  // Position of the best match locale in list of available locales.
  int position = -1;
  const char* language = GetLanguageException(input_locale.getLanguage());
  const char* script = input_locale.getScript();
  const char* region = input_locale.getCountry();
  for (int32_t i = 0; i < available_count; ++i) {
    int current_score = 0;
    int sign =
        CompareLocaleSubtags(language, available_locales[i].getLanguage());
    current_score += sign * kLanguageWeight;

    sign = CompareLocaleSubtags(script, available_locales[i].getScript());
    current_score += sign * kScriptWeight;

    sign = CompareLocaleSubtags(region, available_locales[i].getCountry());
    current_score += sign * kRegionWeight;

    if (current_score >= kThreshold && current_score > result->score) {
      result->score = current_score;
      position = i;
    }
  }

  // Didn't find any good matches so use defaults.
  if (position == -1) return false;

  return BuildLocaleName(available_locales[position].getBaseName(),
                         input_locale.getName(), result);
}

// For some unsupported language subtags it is better to fallback to related
// language that is supported than to default.
static const char* GetLanguageException(const char* language) {
  // Serbo-croatian to Serbian.
  if (!strcmp(language, "sh")) return "sr";

  // Norweigan to Norweiaan to Norwegian Bokmal.
  if (!strcmp(language, "no")) return "nb";

  // Moldavian to Romanian.
  if (!strcmp(language, "mo")) return "ro";

  // Tagalog to Filipino.
  if (!strcmp(language, "tl")) return "fil";

  return language;
}

// Converts user input from BCP47 locale id format to ICU compatible format.
// Returns false if uloc_forLanguageTag call fails or if extension is too long.
static bool BCP47ToICUFormat(const char* locale_id, char* result) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t locale_size = 0;

  char locale[ULOC_FULLNAME_CAPACITY];
  I18NUtils::StrNCopy(locale, ULOC_FULLNAME_CAPACITY, locale_id);

  // uloc_forLanguageTag has a bug where long extension can crash the code.
  // We need to check if extension part of language id conforms to the length.
  // ICU bug: http://bugs.icu-project.org/trac/ticket/8519
  const char* extension = strstr(locale_id, "-u-");
  if (extension != NULL &&
      strlen(extension) > ULOC_KEYWORD_AND_VALUES_CAPACITY) {
    // Truncate to get non-crashing string, but still preserve base language.
    int base_length = strlen(locale_id) - strlen(extension);
    locale[base_length] = '\0';
  }

  uloc_forLanguageTag(locale, result, ULOC_FULLNAME_CAPACITY,
                      &locale_size, &status);
  return !U_FAILURE(status);
}

// Compares locale id subtags.
// Returns 1 for match or -1 for mismatch.
static int CompareLocaleSubtags(const char* lsubtag, const char* rsubtag) {
  return strcmp(lsubtag, rsubtag) == 0 ? 1 : -1;
}

// Builds a BCP47 compliant locale id from base name of matched locale and
// full user specified locale.
// Returns false if uloc_toLanguageTag failed to convert locale id.
// Example:
//   base_name of matched locale (ICU ID): de_DE
//   input_locale_name (ICU ID): de_AT@collation=phonebk
//   result (ICU ID): de_DE@collation=phonebk
//   result (BCP47 ID): de-DE-u-co-phonebk
static bool BuildLocaleName(const char* base_name,
                            const char* input_locale_name,
                            LocaleIDMatch* result) {
  I18NUtils::StrNCopy(result->icu_id, ULOC_LANG_CAPACITY, base_name);

  // Get extensions (if any) from the original locale.
  const char* extension = strchr(input_locale_name, ULOC_KEYWORD_SEPARATOR);
  if (extension != NULL) {
    I18NUtils::StrNCopy(result->icu_id + strlen(base_name),
                        ULOC_KEYWORD_AND_VALUES_CAPACITY, extension);
  } else {
    I18NUtils::StrNCopy(result->icu_id, ULOC_LANG_CAPACITY, base_name);
  }

  // Convert ICU locale name into BCP47 format.
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(result->icu_id, result->bcp47_id,
                     ULOC_FULLNAME_CAPACITY, false, &status);
  return !U_FAILURE(status);
}

} }  // namespace v8::internal
