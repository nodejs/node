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

#include "src/extensions/experimental/break-iterator.h"

#include <string.h>

#include "unicode/brkiter.h"
#include "unicode/locid.h"
#include "unicode/rbbi.h"

namespace v8 {
namespace internal {

v8::Persistent<v8::FunctionTemplate> BreakIterator::break_iterator_template_;

icu::BreakIterator* BreakIterator::UnpackBreakIterator(
    v8::Handle<v8::Object> obj) {
  if (break_iterator_template_->HasInstance(obj)) {
    return static_cast<icu::BreakIterator*>(
        obj->GetPointerFromInternalField(0));
  }

  return NULL;
}

icu::UnicodeString* BreakIterator::ResetAdoptedText(
    v8::Handle<v8::Object> obj, v8::Handle<v8::Value> value) {
  // Get the previous value from the internal field.
  icu::UnicodeString* text = static_cast<icu::UnicodeString*>(
      obj->GetPointerFromInternalField(1));
  delete text;

  // Assign new value to the internal pointer.
  v8::String::Value text_value(value);
  text = new icu::UnicodeString(
      reinterpret_cast<const UChar*>(*text_value), text_value.length());
  obj->SetPointerInInternalField(1, text);

  // Return new unicode string pointer.
  return text;
}

void BreakIterator::DeleteBreakIterator(v8::Persistent<v8::Value> object,
                                        void* param) {
  v8::Persistent<v8::Object> persistent_object =
      v8::Persistent<v8::Object>::Cast(object);

  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a break iterator.
  delete UnpackBreakIterator(persistent_object);

  delete static_cast<icu::UnicodeString*>(
      persistent_object->GetPointerFromInternalField(1));

  // Then dispose of the persistent handle to JS object.
  persistent_object.Dispose();
}

// Throws a JavaScript exception.
static v8::Handle<v8::Value> ThrowUnexpectedObjectError() {
  // Returns undefined, and schedules an exception to be thrown.
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New("BreakIterator method called on an object "
                      "that is not a BreakIterator.")));
}

v8::Handle<v8::Value> BreakIterator::BreakIteratorAdoptText(
    const v8::Arguments& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Text input is required.")));
  }

  icu::BreakIterator* break_iterator = UnpackBreakIterator(args.Holder());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  break_iterator->setText(*ResetAdoptedText(args.Holder(), args[0]));

  return v8::Undefined();
}

v8::Handle<v8::Value> BreakIterator::BreakIteratorFirst(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args.Holder());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  return v8::Int32::New(break_iterator->first());
}

v8::Handle<v8::Value> BreakIterator::BreakIteratorNext(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args.Holder());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  return v8::Int32::New(break_iterator->next());
}

v8::Handle<v8::Value> BreakIterator::BreakIteratorCurrent(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args.Holder());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  return v8::Int32::New(break_iterator->current());
}

v8::Handle<v8::Value> BreakIterator::BreakIteratorBreakType(
    const v8::Arguments& args) {
  icu::BreakIterator* break_iterator = UnpackBreakIterator(args.Holder());
  if (!break_iterator) {
    return ThrowUnexpectedObjectError();
  }

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return v8::Int32::New(UBRK_WORD_NONE);
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return v8::Int32::New(UBRK_WORD_NUMBER);
  } else if (status >= UBRK_WORD_LETTER && status < UBRK_WORD_LETTER_LIMIT) {
    return v8::Int32::New(UBRK_WORD_LETTER);
  } else if (status >= UBRK_WORD_KANA && status < UBRK_WORD_KANA_LIMIT) {
    return v8::Int32::New(UBRK_WORD_KANA);
  } else if (status >= UBRK_WORD_IDEO && status < UBRK_WORD_IDEO_LIMIT) {
    return v8::Int32::New(UBRK_WORD_IDEO);
  } else {
    return v8::Int32::New(-1);
  }
}

v8::Handle<v8::Value> BreakIterator::JSBreakIterator(
    const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Locale and iterator type are required.")));
  }

  v8::String::Utf8Value locale(args[0]);
  icu::Locale icu_locale(*locale);

  UErrorCode status = U_ZERO_ERROR;
  icu::BreakIterator* break_iterator = NULL;
  v8::String::Utf8Value type(args[1]);
  if (!strcmp(*type, "character")) {
    break_iterator =
        icu::BreakIterator::createCharacterInstance(icu_locale, status);
  } else if (!strcmp(*type, "word")) {
    break_iterator =
        icu::BreakIterator::createWordInstance(icu_locale, status);
  } else if (!strcmp(*type, "sentence")) {
    break_iterator =
        icu::BreakIterator::createSentenceInstance(icu_locale, status);
  } else if (!strcmp(*type, "line")) {
    break_iterator =
        icu::BreakIterator::createLineInstance(icu_locale, status);
  } else {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Invalid iterator type.")));
  }

  if (U_FAILURE(status)) {
    delete break_iterator;
    return v8::ThrowException(v8::Exception::Error(
        v8::String::New("Failed to create break iterator.")));
  }

  if (break_iterator_template_.IsEmpty()) {
    v8::Local<v8::FunctionTemplate> raw_template(v8::FunctionTemplate::New());

    raw_template->SetClassName(v8::String::New("v8Locale.v8BreakIterator"));

    // Define internal field count on instance template.
    v8::Local<v8::ObjectTemplate> object_template =
        raw_template->InstanceTemplate();

    // Set aside internal fields for icu break iterator and adopted text.
    object_template->SetInternalFieldCount(2);

    // Define all of the prototype methods on prototype template.
    v8::Local<v8::ObjectTemplate> proto = raw_template->PrototypeTemplate();
    proto->Set(v8::String::New("adoptText"),
               v8::FunctionTemplate::New(BreakIteratorAdoptText));
    proto->Set(v8::String::New("first"),
               v8::FunctionTemplate::New(BreakIteratorFirst));
    proto->Set(v8::String::New("next"),
               v8::FunctionTemplate::New(BreakIteratorNext));
    proto->Set(v8::String::New("current"),
               v8::FunctionTemplate::New(BreakIteratorCurrent));
    proto->Set(v8::String::New("breakType"),
               v8::FunctionTemplate::New(BreakIteratorBreakType));

    break_iterator_template_ =
        v8::Persistent<v8::FunctionTemplate>::New(raw_template);
  }

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object =
      break_iterator_template_->GetFunction()->NewInstance();
  v8::Persistent<v8::Object> wrapper =
      v8::Persistent<v8::Object>::New(local_object);

  // Set break iterator as internal field of the resulting JS object.
  wrapper->SetPointerInInternalField(0, break_iterator);
  // Make sure that the pointer to adopted text is NULL.
  wrapper->SetPointerInInternalField(1, NULL);

  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak(NULL, DeleteBreakIterator);

  return wrapper;
}

} }  // namespace v8::internal
