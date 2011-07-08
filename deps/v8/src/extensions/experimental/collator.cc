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

#include "src/extensions/experimental/collator.h"

#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/ucol.h"

namespace v8 {
namespace internal {

v8::Persistent<v8::FunctionTemplate> Collator::collator_template_;

icu::Collator* Collator::UnpackCollator(v8::Handle<v8::Object> obj) {
  if (collator_template_->HasInstance(obj)) {
    return static_cast<icu::Collator*>(obj->GetPointerFromInternalField(0));
  }

  return NULL;
}

void Collator::DeleteCollator(v8::Persistent<v8::Value> object, void* param) {
  v8::Persistent<v8::Object> persistent_object =
      v8::Persistent<v8::Object>::Cast(object);

  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a collator.
  delete UnpackCollator(persistent_object);

  // Then dispose of the persistent handle to JS object.
  persistent_object.Dispose();
}

// Throws a JavaScript exception.
static v8::Handle<v8::Value> ThrowUnexpectedObjectError() {
  // Returns undefined, and schedules an exception to be thrown.
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New("Collator method called on an object "
                      "that is not a Collator.")));
}

// Extract a boolean option named in |option| and set it to |result|.
// Return true if it's specified. Otherwise, return false.
static bool ExtractBooleanOption(const v8::Local<v8::Object>& options,
                                 const char* option,
                                 bool* result) {
  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Value> value = options->Get(v8::String::New(option));
  if (try_catch.HasCaught()) {
    return false;
  }
  // No need to check if |value| is empty because it's taken care of
  // by TryCatch above.
  if (!value->IsUndefined() && !value->IsNull()) {
    if (value->IsBoolean()) {
      *result = value->BooleanValue();
      return true;
    }
  }
  return false;
}

// When there's an ICU error, throw a JavaScript error with |message|.
static v8::Handle<v8::Value> ThrowExceptionForICUError(const char* message) {
  return v8::ThrowException(v8::Exception::Error(v8::String::New(message)));
}

v8::Handle<v8::Value> Collator::CollatorCompare(const v8::Arguments& args) {
  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Two string arguments are required.")));
  }

  icu::Collator* collator = UnpackCollator(args.Holder());
  if (!collator) {
    return ThrowUnexpectedObjectError();
  }

  v8::String::Value string_value1(args[0]);
  v8::String::Value string_value2(args[1]);
  const UChar* string1 = reinterpret_cast<const UChar*>(*string_value1);
  const UChar* string2 = reinterpret_cast<const UChar*>(*string_value2);
  UErrorCode status = U_ZERO_ERROR;
  UCollationResult result = collator->compare(
      string1, string_value1.length(), string2, string_value2.length(), status);

  if (U_FAILURE(status)) {
    return ThrowExceptionForICUError(
        "Unexpected failure in Collator.compare.");
  }

  return v8::Int32::New(result);
}

v8::Handle<v8::Value> Collator::JSCollator(const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsObject()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Locale and collation options are required.")));
  }

  v8::String::AsciiValue locale(args[0]);
  icu::Locale icu_locale(*locale);

  icu::Collator* collator = NULL;
  UErrorCode status = U_ZERO_ERROR;
  collator = icu::Collator::createInstance(icu_locale, status);

  if (U_FAILURE(status)) {
    delete collator;
    return ThrowExceptionForICUError("Failed to create collator.");
  }

  v8::Local<v8::Object> options(args[1]->ToObject());

  // Below, we change collation options that are explicitly specified
  // by a caller in JavaScript. Otherwise, we don't touch because
  // we don't want to change the locale-dependent default value.
  // The three options below are very likely to have the same default
  // across locales, but I haven't checked them all. Others we may add
  // in the future have certainly locale-dependent default (e.g.
  // caseFirst is upperFirst for Danish while is off for most other locales).

  bool ignore_case, ignore_accents, numeric;

  if (ExtractBooleanOption(options, "ignoreCase", &ignore_case)) {
    // We need to explicitly set the level to secondary to get case ignored.
    // The default L3 ignores UCOL_CASE_LEVEL == UCOL_OFF !
    if (ignore_case) {
      collator->setStrength(icu::Collator::SECONDARY);
    }
    collator->setAttribute(UCOL_CASE_LEVEL, ignore_case ? UCOL_OFF : UCOL_ON,
                           status);
    if (U_FAILURE(status)) {
      delete collator;
      return ThrowExceptionForICUError("Failed to set ignoreCase.");
    }
  }

  // Accents are taken into account with strength secondary or higher.
  if (ExtractBooleanOption(options, "ignoreAccents", &ignore_accents)) {
    if (!ignore_accents) {
      collator->setStrength(icu::Collator::SECONDARY);
    } else {
      collator->setStrength(icu::Collator::PRIMARY);
    }
  }

  if (ExtractBooleanOption(options, "numeric", &numeric)) {
    collator->setAttribute(UCOL_NUMERIC_COLLATION,
                           numeric ? UCOL_ON : UCOL_OFF, status);
    if (U_FAILURE(status)) {
      delete collator;
      return ThrowExceptionForICUError("Failed to set numeric sort option.");
    }
  }

  if (collator_template_.IsEmpty()) {
    v8::Local<v8::FunctionTemplate> raw_template(v8::FunctionTemplate::New());
    raw_template->SetClassName(v8::String::New("v8Locale.Collator"));

    // Define internal field count on instance template.
    v8::Local<v8::ObjectTemplate> object_template =
        raw_template->InstanceTemplate();

    // Set aside internal fields for icu collator.
    object_template->SetInternalFieldCount(1);

    // Define all of the prototype methods on prototype template.
    v8::Local<v8::ObjectTemplate> proto = raw_template->PrototypeTemplate();
    proto->Set(v8::String::New("compare"),
               v8::FunctionTemplate::New(CollatorCompare));

    collator_template_ =
        v8::Persistent<v8::FunctionTemplate>::New(raw_template);
  }

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object =
      collator_template_->GetFunction()->NewInstance();
  v8::Persistent<v8::Object> wrapper =
      v8::Persistent<v8::Object>::New(local_object);

  // Set collator as internal field of the resulting JS object.
  wrapper->SetPointerInInternalField(0, collator);

  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak(NULL, DeleteCollator);

  return wrapper;
}

} }  // namespace v8::internal
