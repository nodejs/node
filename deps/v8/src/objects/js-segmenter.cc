// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-segmenter.h"

#include <map>
#include <memory>
#include <string>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

JSSegmenter::Granularity JSSegmenter::GetGranularity(const char* str) {
  if (strcmp(str, "grapheme") == 0) return JSSegmenter::Granularity::GRAPHEME;
  if (strcmp(str, "word") == 0) return JSSegmenter::Granularity::WORD;
  if (strcmp(str, "sentence") == 0) return JSSegmenter::Granularity::SENTENCE;
  UNREACHABLE();
}

MaybeHandle<JSSegmenter> JSSegmenter::Initialize(
    Isolate* isolate, Handle<JSSegmenter> segmenter_holder,
    Handle<Object> locales, Handle<Object> input_options) {
  segmenter_holder->set_flags(0);

  // 3. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSSegmenter>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 11. If options is undefined, then
  Handle<JSReceiver> options;
  if (input_options->IsUndefined(isolate)) {
    // 11. a. Let options be ObjectCreate(null).
    options = isolate->factory()->NewJSObjectWithNullProto();
    // 12. Else
  } else {
    // 23. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               Object::ToObject(isolate, input_options),
                               JSSegmenter);
  }

  // 4. Let opt be a new Record.
  // 5. Let matcher be ? GetOption(options, "localeMatcher", "string",
  // « "lookup", "best fit" », "best fit").
  // 6. Set opt.[[localeMatcher]] to matcher.
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.Segmenter");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSSegmenter>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 9. Let r be ResolveLocale(%Segmenter%.[[AvailableLocales]],
  // requestedLocales, opt, %Segmenter%.[[RelevantExtensionKeys]]).
  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSSegmenter::GetAvailableLocales(),
                          requested_locales, matcher, {});

  // 10. Set segmenter.[[Locale]] to the value of r.[[Locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  segmenter_holder->set_locale(*locale_str);

  // 13. Let granularity be ? GetOption(options, "granularity", "string", «
  // "grapheme", "word", "sentence" », "grapheme").
  Maybe<Granularity> maybe_granularity = Intl::GetStringOption<Granularity>(
      isolate, options, "granularity", "Intl.Segmenter",
      {"grapheme", "word", "sentence"},
      {Granularity::GRAPHEME, Granularity::WORD, Granularity::SENTENCE},
      Granularity::GRAPHEME);
  MAYBE_RETURN(maybe_granularity, MaybeHandle<JSSegmenter>());
  Granularity granularity_enum = maybe_granularity.FromJust();

  // 14. Set segmenter.[[SegmenterGranularity]] to granularity.
  segmenter_holder->set_granularity(granularity_enum);

  icu::Locale icu_locale = r.icu_locale;
  DCHECK(!icu_locale.isBogus());

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::BreakIterator> icu_break_iterator;

  switch (granularity_enum) {
    case Granularity::GRAPHEME:
      icu_break_iterator.reset(
          icu::BreakIterator::createCharacterInstance(icu_locale, status));
      break;
    case Granularity::WORD:
      icu_break_iterator.reset(
          icu::BreakIterator::createWordInstance(icu_locale, status));
      break;
    case Granularity::SENTENCE:
      icu_break_iterator.reset(
          icu::BreakIterator::createSentenceInstance(icu_locale, status));
      break;
    case Granularity::COUNT:
      UNREACHABLE();
  }

  CHECK(U_SUCCESS(status));
  CHECK_NOT_NULL(icu_break_iterator.get());

  Handle<Managed<icu::BreakIterator>> managed_break_iterator =
      Managed<icu::BreakIterator>::FromUniquePtr(isolate, 0,
                                                 std::move(icu_break_iterator));

  segmenter_holder->set_icu_break_iterator(*managed_break_iterator);
  return segmenter_holder;
}

// ecma402 #sec-Intl.Segmenter.prototype.resolvedOptions
Handle<JSObject> JSSegmenter::ResolvedOptions(
    Isolate* isolate, Handle<JSSegmenter> segmenter_holder) {
  Factory* factory = isolate->factory();
  // 3. Let options be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  // 4. For each row of Table 1, except the header row, do
  // a. Let p be the Property value of the current row.
  // b. Let v be the value of pr's internal slot whose name is the Internal Slot
  //    value of the current row.
  //
  // c. If v is not undefined, then
  //  i. Perform ! CreateDataPropertyOrThrow(options, p, v).
  //    Table 1: Resolved Options of Segmenter Instances
  //     Internal Slot                 Property
  //     [[Locale]]                    "locale"
  //     [[SegmenterGranularity]]      "granularity"

  Handle<String> locale(segmenter_holder->locale(), isolate);
  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  JSObject::AddProperty(isolate, result, factory->granularity_string(),
                        segmenter_holder->GranularityAsString(), NONE);
  // 5. Return options.
  return result;
}

Handle<String> JSSegmenter::GranularityAsString() const {
  switch (granularity()) {
    case Granularity::GRAPHEME:
      return GetReadOnlyRoots().grapheme_string_handle();
    case Granularity::WORD:
      return GetReadOnlyRoots().word_string_handle();
    case Granularity::SENTENCE:
      return GetReadOnlyRoots().sentence_string_handle();
    case Granularity::COUNT:
      UNREACHABLE();
  }
}

const std::set<std::string>& JSSegmenter::GetAvailableLocales() {
  static base::LazyInstance<Intl::AvailableLocales<icu::BreakIterator>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

}  // namespace internal
}  // namespace v8
