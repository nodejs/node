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

#include "collator.h"

#include "i18n-utils.h"
#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/ucol.h"

namespace v8_i18n {

static icu::Collator* InitializeCollator(
    v8::Handle<v8::String>, v8::Handle<v8::Object>, v8::Handle<v8::Object>);

static icu::Collator* CreateICUCollator(
    const icu::Locale&, v8::Handle<v8::Object>);

static bool SetBooleanAttribute(
    UColAttribute, const char*, v8::Handle<v8::Object>, icu::Collator*);

static void SetResolvedSettings(
    const icu::Locale&, icu::Collator*, v8::Handle<v8::Object>);

static void SetBooleanSetting(
    UColAttribute, icu::Collator*, const char*, v8::Handle<v8::Object>);

icu::Collator* Collator::UnpackCollator(v8::Handle<v8::Object> obj) {
  v8::HandleScope handle_scope;

  if (obj->HasOwnProperty(v8::String::New("collator"))) {
    return static_cast<icu::Collator*>(
        obj->GetAlignedPointerFromInternalField(0));
  }

  return NULL;
}

void Collator::DeleteCollator(v8::Isolate* isolate,
                              v8::Persistent<v8::Object>* object,
                              void* param) {
  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a collator.
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> handle = v8::Local<v8::Object>::New(isolate, *object);
  delete UnpackCollator(handle);

  // Then dispose of the persistent handle to JS object.
  object->Dispose(isolate);
}

// Throws a JavaScript exception.
static v8::Handle<v8::Value> ThrowUnexpectedObjectError() {
  // Returns undefined, and schedules an exception to be thrown.
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New("Collator method called on an object "
                      "that is not a Collator.")));
}

// When there's an ICU error, throw a JavaScript error with |message|.
static v8::Handle<v8::Value> ThrowExceptionForICUError(const char* message) {
  return v8::ThrowException(v8::Exception::Error(v8::String::New(message)));
}

// static
void Collator::JSInternalCompare(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsObject() ||
      !args[1]->IsString() || !args[2]->IsString()) {
    v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Collator and two string arguments are required.")));
    return;
  }

  icu::Collator* collator = UnpackCollator(args[0]->ToObject());
  if (!collator) {
    ThrowUnexpectedObjectError();
    return;
  }

  v8::String::Value string_value1(args[1]);
  v8::String::Value string_value2(args[2]);
  const UChar* string1 = reinterpret_cast<const UChar*>(*string_value1);
  const UChar* string2 = reinterpret_cast<const UChar*>(*string_value2);
  UErrorCode status = U_ZERO_ERROR;
  UCollationResult result = collator->compare(
      string1, string_value1.length(), string2, string_value2.length(), status);

  if (U_FAILURE(status)) {
    ThrowExceptionForICUError(
        "Internal error. Unexpected failure in Collator.compare.");
    return;
  }

  args.GetReturnValue().Set(result);
}

void Collator::JSCreateCollator(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsString() || !args[1]->IsObject() ||
      !args[2]->IsObject()) {
    v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Internal error, wrong parameters.")));
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::ObjectTemplate> intl_collator_template =
      Utils::GetTemplate(isolate);

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object = intl_collator_template->NewInstance();
  // But the handle shouldn't be empty.
  // That can happen if there was a stack overflow when creating the object.
  if (local_object.IsEmpty()) {
    args.GetReturnValue().Set(local_object);
    return;
  }

  // Set collator as internal field of the resulting JS object.
  icu::Collator* collator = InitializeCollator(
      args[0]->ToString(), args[1]->ToObject(), args[2]->ToObject());

  if (!collator) {
    v8::ThrowException(v8::Exception::Error(v8::String::New(
        "Internal error. Couldn't create ICU collator.")));
    return;
  } else {
    local_object->SetAlignedPointerInInternalField(0, collator);

    // Make it safer to unpack later on.
    v8::TryCatch try_catch;
    local_object->Set(v8::String::New("collator"), v8::String::New("valid"));
    if (try_catch.HasCaught()) {
      v8::ThrowException(v8::Exception::Error(
          v8::String::New("Internal error, couldn't set property.")));
      return;
    }
  }

  v8::Persistent<v8::Object> wrapper(isolate, local_object);
  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak<void>(NULL, &DeleteCollator);
  args.GetReturnValue().Set(wrapper);
  wrapper.ClearAndLeak();
}

static icu::Collator* InitializeCollator(v8::Handle<v8::String> locale,
                                         v8::Handle<v8::Object> options,
                                         v8::Handle<v8::Object> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::AsciiValue bcp47_locale(locale);
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::Collator* collator = CreateICUCollator(icu_locale, options);
  if (!collator) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    collator = CreateICUCollator(no_extension_locale, options);

    // Set resolved settings (pattern, numbering system).
    SetResolvedSettings(no_extension_locale, collator, resolved);
  } else {
    SetResolvedSettings(icu_locale, collator, resolved);
  }

  return collator;
}

static icu::Collator* CreateICUCollator(
    const icu::Locale& icu_locale, v8::Handle<v8::Object> options) {
  // Make collator from options.
  icu::Collator* collator = NULL;
  UErrorCode status = U_ZERO_ERROR;
  collator = icu::Collator::createInstance(icu_locale, status);

  if (U_FAILURE(status)) {
    delete collator;
    return NULL;
  }

  // Set flags first, and then override them with sensitivity if necessary.
  SetBooleanAttribute(UCOL_NUMERIC_COLLATION, "numeric", options, collator);

  // Normalization is always on, by the spec. We are free to optimize
  // if the strings are already normalized (but we don't have a way to tell
  // that right now).
  collator->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);

  icu::UnicodeString case_first;
  if (Utils::ExtractStringSetting(options, "caseFirst", &case_first)) {
    if (case_first == UNICODE_STRING_SIMPLE("upper")) {
      collator->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST, status);
    } else if (case_first == UNICODE_STRING_SIMPLE("lower")) {
      collator->setAttribute(UCOL_CASE_FIRST, UCOL_LOWER_FIRST, status);
    } else {
      // Default (false/off).
      collator->setAttribute(UCOL_CASE_FIRST, UCOL_OFF, status);
    }
  }

  icu::UnicodeString sensitivity;
  if (Utils::ExtractStringSetting(options, "sensitivity", &sensitivity)) {
    if (sensitivity == UNICODE_STRING_SIMPLE("base")) {
      collator->setStrength(icu::Collator::PRIMARY);
    } else if (sensitivity == UNICODE_STRING_SIMPLE("accent")) {
      collator->setStrength(icu::Collator::SECONDARY);
    } else if (sensitivity == UNICODE_STRING_SIMPLE("case")) {
      collator->setStrength(icu::Collator::PRIMARY);
      collator->setAttribute(UCOL_CASE_LEVEL, UCOL_ON, status);
    } else {
      // variant (default)
      collator->setStrength(icu::Collator::TERTIARY);
    }
  }

  bool ignore;
  if (Utils::ExtractBooleanSetting(options, "ignorePunctuation", &ignore)) {
    if (ignore) {
      collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, status);
    }
  }

  return collator;
}

static bool SetBooleanAttribute(UColAttribute attribute,
                                const char* name,
                                v8::Handle<v8::Object> options,
                                icu::Collator* collator) {
  UErrorCode status = U_ZERO_ERROR;
  bool result;
  if (Utils::ExtractBooleanSetting(options, name, &result)) {
    collator->setAttribute(attribute, result ? UCOL_ON : UCOL_OFF, status);
    if (U_FAILURE(status)) {
      return false;
    }
  }

  return true;
}

static void SetResolvedSettings(const icu::Locale& icu_locale,
                                icu::Collator* collator,
                                v8::Handle<v8::Object> resolved) {
  SetBooleanSetting(UCOL_NUMERIC_COLLATION, collator, "numeric", resolved);

  UErrorCode status = U_ZERO_ERROR;

  switch (collator->getAttribute(UCOL_CASE_FIRST, status)) {
    case UCOL_LOWER_FIRST:
      resolved->Set(v8::String::New("caseFirst"), v8::String::New("lower"));
      break;
    case UCOL_UPPER_FIRST:
      resolved->Set(v8::String::New("caseFirst"), v8::String::New("upper"));
      break;
    default:
      resolved->Set(v8::String::New("caseFirst"), v8::String::New("false"));
  }

  switch (collator->getAttribute(UCOL_STRENGTH, status)) {
    case UCOL_PRIMARY: {
      resolved->Set(v8::String::New("strength"), v8::String::New("primary"));

      // case level: true + s1 -> case, s1 -> base.
      if (UCOL_ON == collator->getAttribute(UCOL_CASE_LEVEL, status)) {
        resolved->Set(v8::String::New("sensitivity"), v8::String::New("case"));
      } else {
        resolved->Set(v8::String::New("sensitivity"), v8::String::New("base"));
      }
      break;
    }
    case UCOL_SECONDARY:
      resolved->Set(v8::String::New("strength"), v8::String::New("secondary"));
      resolved->Set(v8::String::New("sensitivity"), v8::String::New("accent"));
      break;
    case UCOL_TERTIARY:
      resolved->Set(v8::String::New("strength"), v8::String::New("tertiary"));
      resolved->Set(v8::String::New("sensitivity"), v8::String::New("variant"));
      break;
    case UCOL_QUATERNARY:
      // We shouldn't get quaternary and identical from ICU, but if we do
      // put them into variant.
      resolved->Set(v8::String::New("strength"), v8::String::New("quaternary"));
      resolved->Set(v8::String::New("sensitivity"), v8::String::New("variant"));
      break;
    default:
      resolved->Set(v8::String::New("strength"), v8::String::New("identical"));
      resolved->Set(v8::String::New("sensitivity"), v8::String::New("variant"));
  }

  if (UCOL_SHIFTED == collator->getAttribute(UCOL_ALTERNATE_HANDLING, status)) {
    resolved->Set(v8::String::New("ignorePunctuation"),
                  v8::Boolean::New(true));
  } else {
    resolved->Set(v8::String::New("ignorePunctuation"),
                  v8::Boolean::New(false));
  }

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    resolved->Set(v8::String::New("locale"), v8::String::New(result));
  } else {
    // This would never happen, since we got the locale from ICU.
    resolved->Set(v8::String::New("locale"), v8::String::New("und"));
  }
}

static void SetBooleanSetting(UColAttribute attribute,
                              icu::Collator* collator,
                              const char* property,
                              v8::Handle<v8::Object> resolved) {
  UErrorCode status = U_ZERO_ERROR;
  if (UCOL_ON == collator->getAttribute(attribute, status)) {
    resolved->Set(v8::String::New(property), v8::Boolean::New(true));
  } else {
    resolved->Set(v8::String::New(property), v8::Boolean::New(false));
  }
}

}  // namespace v8_i18n
