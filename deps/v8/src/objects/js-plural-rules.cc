// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-plural-rules.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/js-plural-rules-internal.h"
#include "src/objects/managed-inl.h"
#include "src/objects/option-utils.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "unicode/locid.h"
#include "unicode/plurrule.h"
#include "unicode/strenum.h"
#pragma GCC diagnostic pop

namespace v8 {
namespace internal {

Handle<String> JSPluralRules::TypeAsString(Isolate* isolate) const {
  switch (type()) {
    case Type::CARDINAL:
      return isolate->factory()->cardinal_string();
    case Type::ORDINAL:
      return isolate->factory()->ordinal_string();
  }
  UNREACHABLE();
}

// static
MaybeDirectHandle<JSPluralRules> JSPluralRules::New(
    Isolate* isolate, DirectHandle<Map> map, DirectHandle<Object> locales,
    DirectHandle<Object> options_obj, const char* service) {
  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, DirectHandle<JSPluralRules>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 2. Set options to ? CoerceOptionsToObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, CoerceOptionsToObject(isolate, options_obj, service));

  // 5. Let matcher be ? GetOption(options, "localeMatcher", "string",
  // « "lookup", "best fit" », "best fit").
  // 6. Set opt.[[localeMatcher]] to matcher.
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, service);
  MAYBE_RETURN(maybe_locale_matcher, MaybeDirectHandle<JSPluralRules>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 7. Let t be ? GetOption(options, "type", "string", « "cardinal",
  // "ordinal" », "cardinal").
  Maybe<Type> maybe_type = GetStringOption<Type>(
      isolate, options, isolate->factory()->type_string(), service,
      std::to_array<const std::string_view>({"cardinal", "ordinal"}),
      std::array{Type::CARDINAL, Type::ORDINAL}, Type::CARDINAL);
  MAYBE_RETURN(maybe_type, MaybeDirectHandle<JSPluralRules>());
  Type type = maybe_type.FromJust();

  Intl::Notation notation = Intl::Notation::STANDARD;
  // 9. Let notation be ? GetOption(options, "notation", "string", «
  // "standard", "scientific",  "engineering", "compact" », "standard").
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, notation,
      GetStringOption<Intl::Notation>(
          isolate, options, isolate->factory()->notation_string(), service,
          std::to_array<const std::string_view>(
              {"standard", "scientific", "engineering", "compact"}),
          std::array{Intl::Notation::STANDARD, Intl::Notation::SCIENTIFIC,
                     Intl::Notation::ENGINEERING, Intl::Notation::COMPACT},
          Intl::Notation::STANDARD));
  // 10. Set pluralRules.[[Notation]] to notation.
  // 28. Let compactDisplay be ? GetOption(options, "compactDisplay",
  // "string", « "short", "long" »,  "short").
  Maybe<Intl::CompactDisplay> maybe_compact_display =
      GetStringOption<Intl::CompactDisplay>(
          isolate, options, isolate->factory()->compactDisplay_string(),
          service, std::to_array<const std::string_view>({"short", "long"}),
          std::array{Intl::CompactDisplay::SHORT, Intl::CompactDisplay::LONG},
          Intl::CompactDisplay::SHORT);
  MAYBE_RETURN(maybe_compact_display, MaybeDirectHandle<JSPluralRules>());
  Intl::CompactDisplay compact_display = maybe_compact_display.FromJust();

  // Note: The spec says we should do ResolveLocale after performing
  // SetNumberFormatDigitOptions but we need the locale to create all
  // the ICU data structures.
  //
  // This isn't observable so we aren't violating the spec.

  // 11. Let r be ResolveLocale(%PluralRules%.[[AvailableLocales]],
  // requestedLocales, opt, %PluralRules%.[[RelevantExtensionKeys]],
  // localeData).
  Intl::ResolvedLocale r;
  if (!Intl::ResolveLocale(isolate, JSPluralRules::GetAvailableLocales(),
                           requested_locales, matcher, {})
           .To(&r)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }
  DirectHandle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());

  // 11. Perform ? SetNumberFormatDigitOptions(pluralRules, options, 0, 3,
  // notation).
  Maybe<Intl::NumberFormatDigitOptions> maybe_digit_options =
      Intl::SetNumberFormatDigitOptions(
          isolate, options, 0, 3, notation == Intl::Notation::COMPACT, service);
  MAYBE_RETURN(maybe_digit_options, MaybeDirectHandle<JSPluralRules>());
  Intl::NumberFormatDigitOptions digit_options = maybe_digit_options.FromJust();

  std::unique_ptr<PluralRulesInternal> internal = PluralRulesInternal::Create(
      isolate, r, type, notation, compact_display, digit_options);
  if (internal == nullptr) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }

  DirectHandle<CppGCManaged<PluralRulesInternal>> managed_internal =
      CppGCManaged<PluralRulesInternal>::Create(isolate, 0,
                                                std::move(internal));

  // Now all properties are ready, so we can allocate the result object.
  DirectHandle<JSPluralRules> plural_rules = Cast<JSPluralRules>(
      isolate->factory()->NewFastOrSlowJSObjectFromMap(map));
  DisallowGarbageCollection no_gc;
  plural_rules->set_flags(0);

  // 8. Set pluralRules.[[Type]] to t.
  plural_rules->set_type(type);

  // 12. Set pluralRules.[[Locale]] to the value of r.[[locale]].
  plural_rules->set_locale(*locale_str);

  plural_rules->set_internal(*managed_internal);

  // 13. Return pluralRules.
  return plural_rules;
}

MaybeDirectHandle<String> JSPluralRules::ResolvePlural(
    Isolate* isolate, DirectHandle<JSPluralRules> plural_rules, double number) {
  CppGCManaged<PluralRulesInternal>::Ptr internal =
      plural_rules->internal()->ptr();
  DCHECK_NOT_NULL(internal.raw());
  return internal->Select(isolate, number);
}

MaybeDirectHandle<String> JSPluralRules::ResolvePluralRange(
    Isolate* isolate, DirectHandle<JSPluralRules> plural_rules, double x,
    double y) {
  CppGCManaged<PluralRulesInternal>::Ptr internal =
      plural_rules->internal()->ptr();
  DCHECK_NOT_NULL(internal.raw());
  return internal->SelectRange(isolate, plural_rules->locale(), x, y);
}

namespace {

void CreateDataPropertyForOptions(Isolate* isolate,
                                  DirectHandle<JSObject> options,
                                  DirectHandle<Object> value, const char* key) {
  DirectHandle<String> key_str =
      isolate->factory()->NewStringFromAsciiChecked(key);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  Maybe<bool> maybe = JSReceiver::CreateDataProperty(isolate, options, key_str,
                                                     value, Just(kDontThrow));
  DCHECK(maybe.FromJust());
  USE(maybe);
}

}  // namespace

DirectHandle<JSObject> JSPluralRules::ResolvedOptions(
    Isolate* isolate, DirectHandle<JSPluralRules> plural_rules) {
  DirectHandle<JSObject> options =
      isolate->factory()->NewJSObject(isolate->object_function());

  DirectHandle<String> locale_value(plural_rules->locale(), isolate);
  CreateDataPropertyForOptions(isolate, options, locale_value, "locale");

  CreateDataPropertyForOptions(isolate, options,
                               plural_rules->TypeAsString(isolate), "type");

  CppGCManaged<PluralRulesInternal>::Ptr internal =
      plural_rules->internal()->ptr();
  DCHECK_NOT_NULL(internal.raw());
  internal->PopulateResolvedOptions(isolate, options);

  return options;
}

namespace {

class PluralRulesAvailableLocales {
 public:
  PluralRulesAvailableLocales() {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::StringEnumeration> locales(
        icu::PluralRules::getAvailableLocales(status));
    DCHECK(U_SUCCESS(status));
    int32_t len = 0;
    const char* locale = nullptr;
    while ((locale = locales->next(&len, status)) != nullptr &&
           U_SUCCESS(status)) {
      std::string str(locale);
      if (len > 3) {
        std::replace(str.begin(), str.end(), '_', '-');
      }
      set_.insert(std::move(str));
    }
  }
  const std::set<std::string>& Get() const { return set_; }

 private:
  std::set<std::string> set_;
};

}  // namespace

const std::set<std::string>& JSPluralRules::GetAvailableLocales() {
  static base::LazyInstance<PluralRulesAvailableLocales>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

}  // namespace internal
}  // namespace v8
