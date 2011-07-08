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

#include "src/extensions/experimental/i18n-locale.h"

#include "src/extensions/experimental/i18n-utils.h"
#include "src/extensions/experimental/language-matcher.h"
#include "unicode/locid.h"
#include "unicode/uloc.h"

namespace v8 {
namespace internal {

const char* const I18NLocale::kLocaleID = "localeID";
const char* const I18NLocale::kRegionID = "regionID";
const char* const I18NLocale::kICULocaleID = "icuLocaleID";

v8::Handle<v8::Value> I18NLocale::JSLocale(const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  if (args.Length() != 1 || !args[0]->IsObject()) {
    return v8::Undefined();
  }

  v8::Local<v8::Object> settings = args[0]->ToObject();

  // Get best match for locale.
  v8::TryCatch try_catch;
  v8::Handle<v8::Value> locale_id = settings->Get(v8::String::New(kLocaleID));
  if (try_catch.HasCaught()) {
    return v8::Undefined();
  }

  LocaleIDMatch result;
  if (locale_id->IsArray()) {
    LanguageMatcher::GetBestMatchForPriorityList(
        v8::Handle<v8::Array>::Cast(locale_id), &result);
  } else if (locale_id->IsString()) {
    LanguageMatcher::GetBestMatchForString(locale_id->ToString(), &result);
  } else {
    LanguageMatcher::GetBestMatchForString(v8::String::New(""), &result);
  }

  // Get best match for region.
  char region_id[ULOC_COUNTRY_CAPACITY];
  I18NUtils::StrNCopy(region_id, ULOC_COUNTRY_CAPACITY, "");

  v8::Handle<v8::Value> region = settings->Get(v8::String::New(kRegionID));
  if (try_catch.HasCaught()) {
    return v8::Undefined();
  }

  if (!GetBestMatchForRegionID(result.icu_id, region, region_id)) {
    // Set region id to empty string because region couldn't be inferred.
    I18NUtils::StrNCopy(region_id, ULOC_COUNTRY_CAPACITY, "");
  }

  // Build JavaScript object that contains bcp and icu locale ID and region ID.
  v8::Handle<v8::Object> locale = v8::Object::New();
  locale->Set(v8::String::New(kLocaleID), v8::String::New(result.bcp47_id));
  locale->Set(v8::String::New(kICULocaleID), v8::String::New(result.icu_id));
  locale->Set(v8::String::New(kRegionID), v8::String::New(region_id));

  return handle_scope.Close(locale);
}

bool I18NLocale::GetBestMatchForRegionID(
    const char* locale_id, v8::Handle<v8::Value> region_id, char* result) {
  if (region_id->IsString() && region_id->ToString()->Length() != 0) {
    icu::Locale user_locale(
        icu::Locale("und", *v8::String::Utf8Value(region_id->ToString())));
    I18NUtils::StrNCopy(
        result, ULOC_COUNTRY_CAPACITY, user_locale.getCountry());
    return true;
  }
  // Maximize locale_id to infer the region (e.g. expand "de" to "de-Latn-DE"
  // and grab "DE" from the result).
  UErrorCode status = U_ZERO_ERROR;
  char maximized_locale[ULOC_FULLNAME_CAPACITY];
  uloc_addLikelySubtags(
      locale_id, maximized_locale, ULOC_FULLNAME_CAPACITY, &status);
  uloc_getCountry(maximized_locale, result, ULOC_COUNTRY_CAPACITY, &status);

  return !U_FAILURE(status);
}

} }  // namespace v8::internal
