// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-break-iterator.h"

#include "src/objects/intl-objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-break-iterator-inl.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

JSV8BreakIterator::Type JSV8BreakIterator::getType(const char* str) {
  if (strcmp(str, "character") == 0) return Type::CHARACTER;
  if (strcmp(str, "word") == 0) return Type::WORD;
  if (strcmp(str, "sentence") == 0) return Type::SENTENCE;
  if (strcmp(str, "line") == 0) return Type::LINE;
  UNREACHABLE();
}

MaybeHandle<JSV8BreakIterator> JSV8BreakIterator::Initialize(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator_holder,
    Handle<Object> locales, Handle<Object> options_obj) {
  Factory* factory = isolate->factory();

  Handle<JSReceiver> options;
  if (options_obj->IsUndefined(isolate)) {
    options = factory->NewJSObjectWithNullProto();
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, options,
        Object::ToObject(isolate, options_obj, "Intl.JSV8BreakIterator"),
        JSV8BreakIterator);
  }

  // Extract locale string
  Handle<JSObject> r;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, r,
      Intl::ResolveLocale(isolate, "breakiterator", locales, options),
      JSV8BreakIterator);
  Handle<Object> locale_obj =
      JSObject::GetDataProperty(r, factory->locale_string());
  CHECK(locale_obj->IsString());
  Handle<String> locale = Handle<String>::cast(locale_obj);

  // Extract type from options
  std::unique_ptr<char[]> type_str = nullptr;
  std::vector<const char*> type_values = {"character", "word", "sentence",
                                          "line"};
  Maybe<bool> maybe_found_type = Intl::GetStringOption(
      isolate, options, "type", type_values, "Intl.v8BreakIterator", &type_str);
  Type type_enum = Type::WORD;
  MAYBE_RETURN(maybe_found_type, MaybeHandle<JSV8BreakIterator>());
  if (maybe_found_type.FromJust()) {
    DCHECK_NOT_NULL(type_str.get());
    type_enum = getType(type_str.get());
  }

  // Construct icu_locale using the locale string
  icu::Locale icu_locale = Intl::CreateICULocale(isolate, locale);
  DCHECK(!icu_locale.isBogus());

  // Construct break_iterator using icu_locale and type
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::BreakIterator> break_iterator = nullptr;
  switch (type_enum) {
    case Type::CHARACTER:
      break_iterator.reset(
          icu::BreakIterator::createCharacterInstance(icu_locale, status));
      break;
    case Type::SENTENCE:
      break_iterator.reset(
          icu::BreakIterator::createSentenceInstance(icu_locale, status));
      break;
    case Type::LINE:
      break_iterator.reset(
          icu::BreakIterator::createLineInstance(icu_locale, status));
      break;
    default:
      break_iterator.reset(
          icu::BreakIterator::createWordInstance(icu_locale, status));
      break;
  }

  // Error handling for break_iterator
  if (U_FAILURE(status)) {
    FATAL("Failed to create ICU break iterator, are ICU data files missing?");
  }
  CHECK_NOT_NULL(break_iterator.get());
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kBreakIterator);

  // Construct managed objects from pointers
  Handle<Managed<icu::BreakIterator>> managed_break_iterator =
      Managed<icu::BreakIterator>::FromUniquePtr(isolate, 0,
                                                 std::move(break_iterator));
  Handle<Managed<icu::UnicodeString>> managed_unicode_string =
      Managed<icu::UnicodeString>::FromRawPtr(isolate, 0, nullptr);

  // Setting fields
  break_iterator_holder->set_locale(*locale);
  break_iterator_holder->set_type(type_enum);
  break_iterator_holder->set_break_iterator(*managed_break_iterator);
  break_iterator_holder->set_unicode_string(*managed_unicode_string);

  // Return break_iterator_holder
  return break_iterator_holder;
}

Handle<JSObject> JSV8BreakIterator::ResolvedOptions(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator) {
  Factory* factory = isolate->factory();

  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  Handle<String> locale(break_iterator->locale(), isolate);

  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  JSObject::AddProperty(isolate, result, factory->type_string(),
                        break_iterator->TypeAsString(), NONE);
  return result;
}

void JSV8BreakIterator::AdoptText(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator_holder,
    Handle<String> text) {
  icu::UnicodeString* u_text;
  int length = text->length();
  text = String::Flatten(isolate, text);
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = text->GetFlatContent();
    std::unique_ptr<uc16[]> sap;
    const UChar* text_value = GetUCharBufferFromFlat(flat, &sap, length);
    u_text = new icu::UnicodeString(text_value, length);
  }

  Handle<Managed<icu::UnicodeString>> new_u_text =
      Managed<icu::UnicodeString>::FromRawPtr(isolate, 0, u_text);
  break_iterator_holder->set_unicode_string(*new_u_text);

  icu::BreakIterator* break_iterator =
      break_iterator_holder->break_iterator()->raw();
  CHECK_NOT_NULL(break_iterator);
  break_iterator->setText(*u_text);
}

Handle<String> JSV8BreakIterator::TypeAsString() const {
  switch (type()) {
    case Type::CHARACTER:
      return GetReadOnlyRoots().character_string_handle();
    case Type::WORD:
      return GetReadOnlyRoots().word_string_handle();
    case Type::SENTENCE:
      return GetReadOnlyRoots().sentence_string_handle();
    case Type::LINE:
      return GetReadOnlyRoots().line_string_handle();
    case Type::COUNT:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
