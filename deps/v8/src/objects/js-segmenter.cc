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

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/objects/managed.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

JSSegmenter::LineBreakStyle JSSegmenter::GetLineBreakStyle(const char* str) {
  if (strcmp(str, "strict") == 0) return JSSegmenter::LineBreakStyle::STRICT;
  if (strcmp(str, "normal") == 0) return JSSegmenter::LineBreakStyle::NORMAL;
  if (strcmp(str, "loose") == 0) return JSSegmenter::LineBreakStyle::LOOSE;
  UNREACHABLE();
}

JSSegmenter::Granularity JSSegmenter::GetGranularity(const char* str) {
  if (strcmp(str, "grapheme") == 0) return JSSegmenter::Granularity::GRAPHEME;
  if (strcmp(str, "word") == 0) return JSSegmenter::Granularity::WORD;
  if (strcmp(str, "sentence") == 0) return JSSegmenter::Granularity::SENTENCE;
  if (strcmp(str, "line") == 0) return JSSegmenter::Granularity::LINE;
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
  const std::vector<const char*> values = {"lookup", "best fit"};
  std::unique_ptr<char[]> matcher_str = nullptr;
  Intl::MatcherOption matcher = Intl::MatcherOption::kBestFit;
  Maybe<bool> found_matcher =
      Intl::GetStringOption(isolate, options, "localeMatcher", values,
                            "Intl.Segmenter", &matcher_str);
  MAYBE_RETURN(found_matcher, MaybeHandle<JSSegmenter>());
  if (found_matcher.FromJust()) {
    DCHECK_NOT_NULL(matcher_str.get());
    if (strcmp(matcher_str.get(), "lookup") == 0) {
      matcher = Intl::MatcherOption::kLookup;
    }
  }

  // 8. Set opt.[[lb]] to lineBreakStyle.

  // 9. Let r be ResolveLocale(%Segmenter%.[[AvailableLocales]],
  // requestedLocales, opt, %Segmenter%.[[RelevantExtensionKeys]]).
  std::set<std::string> available_locales =
      Intl::GetAvailableLocales(Intl::ICUService::kSegmenter);
  Intl::ResolvedLocale r = Intl::ResolveLocale(isolate, available_locales,
                                               requested_locales, matcher, {});

  // 7. Let lineBreakStyle be ? GetOption(options, "lineBreakStyle", "string", «
  // "strict", "normal", "loose" », "normal").
  std::unique_ptr<char[]> line_break_style_str = nullptr;
  const std::vector<const char*> line_break_style_values = {"strict", "normal",
                                                            "loose"};
  Maybe<bool> maybe_found_line_break_style = Intl::GetStringOption(
      isolate, options, "lineBreakStyle", line_break_style_values,
      "Intl.Segmenter", &line_break_style_str);
  LineBreakStyle line_break_style_enum = LineBreakStyle::NORMAL;
  MAYBE_RETURN(maybe_found_line_break_style, MaybeHandle<JSSegmenter>());
  if (maybe_found_line_break_style.FromJust()) {
    DCHECK_NOT_NULL(line_break_style_str.get());
    line_break_style_enum = GetLineBreakStyle(line_break_style_str.get());
  }

  // 10. Set segmenter.[[Locale]] to the value of r.[[Locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  segmenter_holder->set_locale(*locale_str);

  // 13. Let granularity be ? GetOption(options, "granularity", "string", «
  // "grapheme", "word", "sentence", "line" », "grapheme").

  std::unique_ptr<char[]> granularity_str = nullptr;
  const std::vector<const char*> granularity_values = {"grapheme", "word",
                                                       "sentence", "line"};
  Maybe<bool> maybe_found_granularity =
      Intl::GetStringOption(isolate, options, "granularity", granularity_values,
                            "Intl.Segmenter", &granularity_str);
  Granularity granularity_enum = Granularity::GRAPHEME;
  MAYBE_RETURN(maybe_found_granularity, MaybeHandle<JSSegmenter>());
  if (maybe_found_granularity.FromJust()) {
    DCHECK_NOT_NULL(granularity_str.get());
    granularity_enum = GetGranularity(granularity_str.get());
  }

  // 14. Set segmenter.[[SegmenterGranularity]] to granularity.
  segmenter_holder->set_granularity(granularity_enum);

  // 15. If granularity is "line",
  if (granularity_enum == Granularity::LINE) {
    // a. Set segmenter.[[SegmenterLineBreakStyle]] to r.[[lb]].
    segmenter_holder->set_line_break_style(line_break_style_enum);
  } else {
    segmenter_holder->set_line_break_style(LineBreakStyle::NOTSET);
  }

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
    case Granularity::LINE: {
      // 15. If granularity is "line",
      // a. Set segmenter.[[SegmenterLineBreakStyle]] to r.[[lb]].
      const char* key = uloc_toLegacyKey("lb");
      CHECK_NOT_NULL(key);
      const char* value =
          uloc_toLegacyType(key, segmenter_holder->LineBreakStyleAsCString());
      CHECK_NOT_NULL(value);
      UErrorCode status = U_ZERO_ERROR;
      icu_locale.setKeywordValue(key, value, status);
      CHECK(U_SUCCESS(status));
      icu_break_iterator.reset(
          icu::BreakIterator::createLineInstance(icu_locale, status));
      break;
    }
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

Handle<JSObject> JSSegmenter::ResolvedOptions(
    Isolate* isolate, Handle<JSSegmenter> segmenter_holder) {
  Factory* factory = isolate->factory();
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  Handle<String> locale(segmenter_holder->locale(), isolate);
  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  if (segmenter_holder->line_break_style() != LineBreakStyle::NOTSET) {
    JSObject::AddProperty(isolate, result, factory->lineBreakStyle_string(),
                          segmenter_holder->LineBreakStyleAsString(), NONE);
  }
  JSObject::AddProperty(isolate, result, factory->granularity_string(),
                        segmenter_holder->GranularityAsString(), NONE);
  return result;
}

const char* JSSegmenter::LineBreakStyleAsCString() const {
  switch (line_break_style()) {
    case LineBreakStyle::STRICT:
      return "strict";
    case LineBreakStyle::NORMAL:
      return "normal";
    case LineBreakStyle::LOOSE:
      return "loose";
    case LineBreakStyle::COUNT:
    case LineBreakStyle::NOTSET:
      UNREACHABLE();
  }
}

Handle<String> JSSegmenter::LineBreakStyleAsString() const {
  switch (line_break_style()) {
    case LineBreakStyle::STRICT:
      return GetReadOnlyRoots().strict_string_handle();
    case LineBreakStyle::NORMAL:
      return GetReadOnlyRoots().normal_string_handle();
    case LineBreakStyle::LOOSE:
      return GetReadOnlyRoots().loose_string_handle();
    case LineBreakStyle::COUNT:
    case LineBreakStyle::NOTSET:
      UNREACHABLE();
  }
}

Handle<String> JSSegmenter::GranularityAsString() const {
  switch (granularity()) {
    case Granularity::GRAPHEME:
      return GetReadOnlyRoots().grapheme_string_handle();
    case Granularity::WORD:
      return GetReadOnlyRoots().word_string_handle();
    case Granularity::SENTENCE:
      return GetReadOnlyRoots().sentence_string_handle();
    case Granularity::LINE:
      return GetReadOnlyRoots().line_string_handle();
    case Granularity::COUNT:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
