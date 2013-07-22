// Copyright 2013 the V8 project authors. All rights reserved.
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
// limitations under the License.

#include "locale.h"

#include <string.h>

#include "unicode/brkiter.h"
#include "unicode/coll.h"
#include "unicode/datefmt.h"
#include "unicode/numfmt.h"
#include "unicode/uloc.h"
#include "unicode/uversion.h"

namespace v8_i18n {

void JSCanonicalizeLanguageTag(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Expect locale id which is a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Locale identifier, as a string, is required.")));
    return;
  }

  UErrorCode error = U_ZERO_ERROR;

  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;

  // Return value which denotes invalid language tag.
  const char* const kInvalidTag = "invalid-tag";

  v8::String::AsciiValue locale_id(args[0]->ToString());
  if (*locale_id == NULL) {
    args.GetReturnValue().Set(v8::String::New(kInvalidTag));
    return;
  }

  uloc_forLanguageTag(*locale_id, icu_result, ULOC_FULLNAME_CAPACITY,
                      &icu_length, &error);
  if (U_FAILURE(error) || icu_length == 0) {
    args.GetReturnValue().Set(v8::String::New(kInvalidTag));
    return;
  }

  char result[ULOC_FULLNAME_CAPACITY];

  // Force strict BCP47 rules.
  uloc_toLanguageTag(icu_result, result, ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error)) {
    args.GetReturnValue().Set(v8::String::New(kInvalidTag));
    return;
  }

  args.GetReturnValue().Set(v8::String::New(result));
}


void JSAvailableLocalesOf(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Expect service name which is a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Service identifier, as a string, is required.")));
    return;
  }

  const icu::Locale* available_locales = NULL;

  int32_t count = 0;
  v8::String::AsciiValue service(args[0]->ToString());
  if (strcmp(*service, "collator") == 0) {
    available_locales = icu::Collator::getAvailableLocales(count);
  } else if (strcmp(*service, "numberformat") == 0) {
    available_locales = icu::NumberFormat::getAvailableLocales(count);
  } else if (strcmp(*service, "dateformat") == 0) {
    available_locales = icu::DateFormat::getAvailableLocales(count);
  } else if (strcmp(*service, "breakiterator") == 0) {
    available_locales = icu::BreakIterator::getAvailableLocales(count);
  }

  v8::TryCatch try_catch;
  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];
  v8::Handle<v8::Object> locales = v8::Object::New();

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error)) {
      // This shouldn't happen, but lets not break the user.
      continue;
    }

    // Index is just a dummy value for the property value.
    locales->Set(v8::String::New(result), v8::Integer::New(i));
    if (try_catch.HasCaught()) {
      // Ignore error, but stop processing and return.
      break;
    }
  }

  args.GetReturnValue().Set(locales);
}


void JSGetDefaultICULocale(const v8::FunctionCallbackInfo<v8::Value>& args) {
  icu::Locale default_locale;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      default_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    args.GetReturnValue().Set(v8::String::New(result));
    return;
  }

  args.GetReturnValue().Set(v8::String::New("und"));
}


void JSGetLanguageTagVariants(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch;

  // Expect an array of strings.
  if (args.Length() != 1 || !args[0]->IsArray()) {
    v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Internal error. Expected Array<String>.")));
    return;
  }

  v8::Local<v8::Array> input = v8::Local<v8::Array>::Cast(args[0]);
  v8::Handle<v8::Array> output = v8::Array::New(input->Length());
  for (unsigned int i = 0; i < input->Length(); ++i) {
    v8::Local<v8::Value> locale_id = input->Get(i);
    if (try_catch.HasCaught()) {
      break;
    }

    if (!locale_id->IsString()) {
      v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Array element is missing "
                          "or it isn't a string.")));
      return;
    }

    v8::String::AsciiValue ascii_locale_id(locale_id);
    if (*ascii_locale_id == NULL) {
      v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Non-ASCII locale identifier.")));
      return;
    }

    UErrorCode error = U_ZERO_ERROR;

    // Convert from BCP47 to ICU format.
    // de-DE-u-co-phonebk -> de_DE@collation=phonebook
    char icu_locale[ULOC_FULLNAME_CAPACITY];
    int icu_locale_length = 0;
    uloc_forLanguageTag(*ascii_locale_id, icu_locale, ULOC_FULLNAME_CAPACITY,
                        &icu_locale_length, &error);
    if (U_FAILURE(error) || icu_locale_length == 0) {
      v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Failed to convert locale to ICU.")));
      return;
    }

    // Maximize the locale.
    // de_DE@collation=phonebook -> de_Latn_DE@collation=phonebook
    char icu_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_addLikelySubtags(
        icu_locale, icu_max_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Remove extensions from maximized locale.
    // de_Latn_DE@collation=phonebook -> de_Latn_DE
    char icu_base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(
        icu_max_locale, icu_base_max_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Get original name without extensions.
    // de_DE@collation=phonebook -> de_DE
    char icu_base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(
        icu_locale, icu_base_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Convert from ICU locale format to BCP47 format.
    // de_Latn_DE -> de-Latn-DE
    char base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(icu_base_max_locale, base_max_locale,
                       ULOC_FULLNAME_CAPACITY, FALSE, &error);

    // de_DE -> de-DE
    char base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(
        icu_base_locale, base_locale, ULOC_FULLNAME_CAPACITY, FALSE, &error);

    if (U_FAILURE(error)) {
      v8::ThrowException(v8::Exception::SyntaxError(
          v8::String::New("Internal error. Couldn't generate maximized "
                          "or base locale.")));
      return;
    }

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("maximized"), v8::String::New(base_max_locale));
    result->Set(v8::String::New("base"), v8::String::New(base_locale));
    if (try_catch.HasCaught()) {
      break;
    }

    output->Set(i, result);
    if (try_catch.HasCaught()) {
      break;
    }
  }

  args.GetReturnValue().Set(output);
}

}  // namespace v8_i18n
