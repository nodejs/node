// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-break-iterator.h"

#include "src/objects/intl-objects.h"
#include "src/objects/js-break-iterator-inl.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

MaybeHandle<JSV8BreakIterator> JSV8BreakIterator::Initialize(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator_holder,
    Handle<Object> locales, Handle<Object> options_obj) {
  Factory* factory = isolate->factory();

  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, MaybeHandle<JSV8BreakIterator>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

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
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.JSV8BreakIterator");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSV8BreakIterator>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSV8BreakIterator::GetAvailableLocales(),
                          requested_locales, matcher, {});

  // Extract type from options
  Maybe<Type> maybe_type = Intl::GetStringOption<Type>(
      isolate, options, "type", "Intl.v8BreakIterator",
      {"word", "character", "sentence", "line"},
      {Type::WORD, Type::CHARACTER, Type::SENTENCE, Type::LINE}, Type::WORD);
  MAYBE_RETURN(maybe_type, MaybeHandle<JSV8BreakIterator>());
  Type type_enum = maybe_type.FromJust();

  icu::Locale icu_locale = r.icu_locale;
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

  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  break_iterator_holder->set_locale(*locale_str);

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
  icu::BreakIterator* break_iterator =
      break_iterator_holder->break_iterator().raw();
  CHECK_NOT_NULL(break_iterator);
  Managed<icu::UnicodeString> unicode_string =
      Intl::SetTextToBreakIterator(isolate, text, break_iterator);
  break_iterator_holder->set_unicode_string(unicode_string);
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

Handle<Object> JSV8BreakIterator::Current(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator) {
  return isolate->factory()->NewNumberFromInt(
      break_iterator->break_iterator().raw()->current());
}

Handle<Object> JSV8BreakIterator::First(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator) {
  return isolate->factory()->NewNumberFromInt(
      break_iterator->break_iterator().raw()->first());
}

Handle<Object> JSV8BreakIterator::Next(
    Isolate* isolate, Handle<JSV8BreakIterator> break_iterator) {
  return isolate->factory()->NewNumberFromInt(
      break_iterator->break_iterator().raw()->next());
}

String JSV8BreakIterator::BreakType(Isolate* isolate,
                                    Handle<JSV8BreakIterator> break_iterator) {
  int32_t status = break_iterator->break_iterator().raw()->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return ReadOnlyRoots(isolate).none_string();
  }
  if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return ReadOnlyRoots(isolate).number_string();
  }
  if (status >= UBRK_WORD_LETTER && status < UBRK_WORD_LETTER_LIMIT) {
    return ReadOnlyRoots(isolate).letter_string();
  }
  if (status >= UBRK_WORD_KANA && status < UBRK_WORD_KANA_LIMIT) {
    return ReadOnlyRoots(isolate).kana_string();
  }
  if (status >= UBRK_WORD_IDEO && status < UBRK_WORD_IDEO_LIMIT) {
    return ReadOnlyRoots(isolate).ideo_string();
  }
  return ReadOnlyRoots(isolate).unknown_string();
}

const std::set<std::string>& JSV8BreakIterator::GetAvailableLocales() {
  static base::LazyInstance<Intl::AvailableLocales<icu::BreakIterator>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

}  // namespace internal
}  // namespace v8
