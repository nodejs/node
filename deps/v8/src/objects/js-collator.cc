// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-collator.h"

#include "src/execution/isolate.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/objects-inl.h"
#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/strenum.h"
#include "unicode/ucol.h"
#include "unicode/udata.h"
#include "unicode/uloc.h"
#include "unicode/utypes.h"

namespace v8 {
namespace internal {

namespace {

enum class Usage {
  SORT,
  SEARCH,
};

enum class Sensitivity {
  kBase,
  kAccent,
  kCase,
  kVariant,
  kUndefined,
};

// enum for "caseFirst" option.
enum class CaseFirst { kUndefined, kUpper, kLower, kFalse };

Maybe<CaseFirst> GetCaseFirst(Isolate* isolate, Handle<JSReceiver> options,
                              const char* method) {
  return Intl::GetStringOption<CaseFirst>(
      isolate, options, "caseFirst", method, {"upper", "lower", "false"},
      {CaseFirst::kUpper, CaseFirst::kLower, CaseFirst::kFalse},
      CaseFirst::kUndefined);
}

// TODO(gsathya): Consider internalizing the value strings.
void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  Handle<String> key, const char* value) {
  CHECK_NOT_NULL(value);
  Handle<String> value_str =
      isolate->factory()->NewStringFromAsciiChecked(value);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  CHECK(JSReceiver::CreateDataProperty(isolate, options, key, value_str,
                                       Just(kDontThrow))
            .FromJust());
}

void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  Handle<String> key, bool value) {
  Handle<Object> value_obj = isolate->factory()->ToBoolean(value);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  CHECK(JSReceiver::CreateDataProperty(isolate, options, key, value_obj,
                                       Just(kDontThrow))
            .FromJust());
}

}  // anonymous namespace

// static
Handle<JSObject> JSCollator::ResolvedOptions(Isolate* isolate,
                                             Handle<JSCollator> collator) {
  Handle<JSObject> options =
      isolate->factory()->NewJSObject(isolate->object_function());

  icu::Collator* icu_collator = collator->icu_collator().raw();
  CHECK_NOT_NULL(icu_collator);

  UErrorCode status = U_ZERO_ERROR;
  bool numeric =
      icu_collator->getAttribute(UCOL_NUMERIC_COLLATION, status) == UCOL_ON;
  CHECK(U_SUCCESS(status));

  const char* case_first = nullptr;
  status = U_ZERO_ERROR;
  switch (icu_collator->getAttribute(UCOL_CASE_FIRST, status)) {
    case UCOL_LOWER_FIRST:
      case_first = "lower";
      break;
    case UCOL_UPPER_FIRST:
      case_first = "upper";
      break;
    default:
      case_first = "false";
  }
  CHECK(U_SUCCESS(status));

  const char* sensitivity = nullptr;
  status = U_ZERO_ERROR;
  switch (icu_collator->getAttribute(UCOL_STRENGTH, status)) {
    case UCOL_PRIMARY: {
      CHECK(U_SUCCESS(status));
      status = U_ZERO_ERROR;
      // case level: true + s1 -> case, s1 -> base.
      if (UCOL_ON == icu_collator->getAttribute(UCOL_CASE_LEVEL, status)) {
        sensitivity = "case";
      } else {
        sensitivity = "base";
      }
      CHECK(U_SUCCESS(status));
      break;
    }
    case UCOL_SECONDARY:
      sensitivity = "accent";
      break;
    case UCOL_TERTIARY:
      sensitivity = "variant";
      break;
    case UCOL_QUATERNARY:
      // We shouldn't get quaternary and identical from ICU, but if we do
      // put them into variant.
      sensitivity = "variant";
      break;
    default:
      sensitivity = "variant";
  }
  CHECK(U_SUCCESS(status));

  status = U_ZERO_ERROR;
  bool ignore_punctuation = icu_collator->getAttribute(UCOL_ALTERNATE_HANDLING,
                                                       status) == UCOL_SHIFTED;
  CHECK(U_SUCCESS(status));

  status = U_ZERO_ERROR;

  icu::Locale icu_locale(icu_collator->getLocale(ULOC_VALID_LOCALE, status));
  CHECK(U_SUCCESS(status));

  const char* collation = "default";
  const char* usage = "sort";
  const char* collation_key = "co";
  status = U_ZERO_ERROR;
  std::string collation_value =
      icu_locale.getUnicodeKeywordValue<std::string>(collation_key, status);

  std::string locale;
  if (U_SUCCESS(status)) {
    if (collation_value == "search") {
      usage = "search";

      // Search is disallowed as a collation value per spec. Let's
      // use `default`, instead.
      //
      // https://tc39.github.io/ecma402/#sec-properties-of-intl-collator-instances
      collation = "default";

      // We clone the icu::Locale because we don't want the
      // icu_collator to be affected when we remove the collation key
      // below.
      icu::Locale new_icu_locale = icu_locale;

      // The spec forbids the search as a collation value in the
      // locale tag, so let's filter it out.
      status = U_ZERO_ERROR;
      new_icu_locale.setUnicodeKeywordValue(collation_key, nullptr, status);
      CHECK(U_SUCCESS(status));

      locale = Intl::ToLanguageTag(new_icu_locale).FromJust();
    } else {
      collation = collation_value.c_str();
      locale = Intl::ToLanguageTag(icu_locale).FromJust();
    }
  } else {
    locale = Intl::ToLanguageTag(icu_locale).FromJust();
  }

  // 5. For each row of Table 2, except the header row, in table order, do
  //    ...
  // Table 2: Resolved Options of Collator Instances
  //  Internal Slot            Property               Extension Key
  //    [[Locale]                "locale"
  //    [[Usage]                 "usage"
  //    [[Sensitivity]]          "sensitivity"
  //    [[IgnorePunctuation]]    "ignorePunctuation"
  //    [[Collation]]            "collation"
  //    [[Numeric]]              "numeric"              kn
  //    [[CaseFirst]]            "caseFirst"            kf
  CreateDataPropertyForOptions(
      isolate, options, isolate->factory()->locale_string(), locale.c_str());
  CreateDataPropertyForOptions(isolate, options,
                               isolate->factory()->usage_string(), usage);
  CreateDataPropertyForOptions(
      isolate, options, isolate->factory()->sensitivity_string(), sensitivity);
  CreateDataPropertyForOptions(isolate, options,
                               isolate->factory()->ignorePunctuation_string(),
                               ignore_punctuation);
  CreateDataPropertyForOptions(
      isolate, options, isolate->factory()->collation_string(), collation);
  CreateDataPropertyForOptions(isolate, options,
                               isolate->factory()->numeric_string(), numeric);
  CreateDataPropertyForOptions(
      isolate, options, isolate->factory()->caseFirst_string(), case_first);
  return options;
}

namespace {

CaseFirst ToCaseFirst(const char* str) {
  if (strcmp(str, "upper") == 0) return CaseFirst::kUpper;
  if (strcmp(str, "lower") == 0) return CaseFirst::kLower;
  if (strcmp(str, "false") == 0) return CaseFirst::kFalse;
  return CaseFirst::kUndefined;
}

UColAttributeValue ToUColAttributeValue(CaseFirst case_first) {
  switch (case_first) {
    case CaseFirst::kUpper:
      return UCOL_UPPER_FIRST;
    case CaseFirst::kLower:
      return UCOL_LOWER_FIRST;
    case CaseFirst::kFalse:
    case CaseFirst::kUndefined:
      return UCOL_OFF;
  }
}

void SetNumericOption(icu::Collator* icu_collator, bool numeric) {
  CHECK_NOT_NULL(icu_collator);
  UErrorCode status = U_ZERO_ERROR;
  icu_collator->setAttribute(UCOL_NUMERIC_COLLATION,
                             numeric ? UCOL_ON : UCOL_OFF, status);
  CHECK(U_SUCCESS(status));
}

void SetCaseFirstOption(icu::Collator* icu_collator, CaseFirst case_first) {
  CHECK_NOT_NULL(icu_collator);
  UErrorCode status = U_ZERO_ERROR;
  icu_collator->setAttribute(UCOL_CASE_FIRST, ToUColAttributeValue(case_first),
                             status);
  CHECK(U_SUCCESS(status));
}

}  // anonymous namespace

// static
MaybeHandle<JSCollator> JSCollator::New(Isolate* isolate, Handle<Map> map,
                                        Handle<Object> locales,
                                        Handle<Object> options_obj,
                                        const char* service) {
  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSCollator>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 2. If options is undefined, then
  if (options_obj->IsUndefined(isolate)) {
    // 2. a. Let options be ObjectCreate(null).
    options_obj = isolate->factory()->NewJSObjectWithNullProto();
  } else {
    // 3. Else
    // 3. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options_obj,
                               Object::ToObject(isolate, options_obj, service),
                               JSCollator);
  }

  // At this point, options_obj can either be a JSObject or a JSProxy only.
  Handle<JSReceiver> options = Handle<JSReceiver>::cast(options_obj);

  // 4. Let usage be ? GetOption(options, "usage", "string", « "sort",
  // "search" », "sort").
  Maybe<Usage> maybe_usage = Intl::GetStringOption<Usage>(
      isolate, options, "usage", service, {"sort", "search"},
      {Usage::SORT, Usage::SEARCH}, Usage::SORT);
  MAYBE_RETURN(maybe_usage, MaybeHandle<JSCollator>());
  Usage usage = maybe_usage.FromJust();

  // 9. Let matcher be ? GetOption(options, "localeMatcher", "string",
  // « "lookup", "best fit" », "best fit").
  // 10. Set opt.[[localeMatcher]] to matcher.
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, service);
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSCollator>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 11. Let numeric be ? GetOption(options, "numeric", "boolean",
  // undefined, undefined).
  // 12. If numeric is not undefined, then
  //    a. Let numeric be ! ToString(numeric).
  //
  // Note: We omit the ToString(numeric) operation as it's not
  // observable. Intl::GetBoolOption returns a Boolean and
  // ToString(Boolean) is not side-effecting.
  //
  // 13. Set opt.[[kn]] to numeric.
  bool numeric;
  Maybe<bool> found_numeric =
      Intl::GetBoolOption(isolate, options, "numeric", service, &numeric);
  MAYBE_RETURN(found_numeric, MaybeHandle<JSCollator>());

  // 14. Let caseFirst be ? GetOption(options, "caseFirst", "string",
  //     « "upper", "lower", "false" », undefined).
  Maybe<CaseFirst> maybe_case_first = GetCaseFirst(isolate, options, service);
  MAYBE_RETURN(maybe_case_first, MaybeHandle<JSCollator>());
  CaseFirst case_first = maybe_case_first.FromJust();

  // The relevant unicode extensions accepted by Collator as specified here:
  // https://tc39.github.io/ecma402/#sec-intl-collator-internal-slots
  //
  // 16. Let relevantExtensionKeys be %Collator%.[[RelevantExtensionKeys]].
  std::set<std::string> relevant_extension_keys{"co", "kn", "kf"};

  // 17. Let r be ResolveLocale(%Collator%.[[AvailableLocales]],
  // requestedLocales, opt, %Collator%.[[RelevantExtensionKeys]],
  // localeData).
  Maybe<Intl::ResolvedLocale> maybe_resolve_locale =
      Intl::ResolveLocale(isolate, JSCollator::GetAvailableLocales(),
                          requested_locales, matcher, relevant_extension_keys);
  if (maybe_resolve_locale.IsNothing()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError),
                    JSCollator);
  }
  Intl::ResolvedLocale r = maybe_resolve_locale.FromJust();

  // 18. Set collator.[[Locale]] to r.[[locale]].
  icu::Locale icu_locale = r.icu_locale;
  DCHECK(!icu_locale.isBogus());

  // 19. Let collation be r.[[co]].

  // 5. Set collator.[[Usage]] to usage.
  //
  // 6. If usage is "sort", then
  //    a. Let localeData be %Collator%.[[SortLocaleData]].
  // 7. Else,
  //    a. Let localeData be %Collator%.[[SearchLocaleData]].
  //
  // The Intl spec doesn't allow us to use "search" as an extension
  // value for collation as per:
  // https://tc39.github.io/ecma402/#sec-intl-collator-internal-slots
  //
  // But the only way to pass the value "search" for collation from
  // the options object to ICU is to use the 'co' extension keyword.
  //
  // This will need to be filtered out when creating the
  // resolvedOptions object.
  if (usage == Usage::SEARCH) {
    UErrorCode status = U_ZERO_ERROR;
    icu_locale.setUnicodeKeywordValue("co", "search", status);
    CHECK(U_SUCCESS(status));
  }

  // 20. If collation is null, let collation be "default".
  // 21. Set collator.[[Collation]] to collation.
  //
  // We don't store the collation value as per the above two steps
  // here. The collation value can be looked up from icu::Collator on
  // demand, as part of Intl.Collator.prototype.resolvedOptions.

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> icu_collator(
      icu::Collator::createInstance(icu_locale, status));
  if (U_FAILURE(status) || icu_collator.get() == nullptr) {
    status = U_ZERO_ERROR;
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    icu_collator.reset(
        icu::Collator::createInstance(no_extension_locale, status));

    if (U_FAILURE(status) || icu_collator.get() == nullptr) {
      THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError),
                      JSCollator);
    }
  }
  DCHECK(U_SUCCESS(status));

  // 22. If relevantExtensionKeys contains "kn", then
  //     a. Set collator.[[Numeric]] to ! SameValue(r.[[kn]], "true").
  //
  // If the numeric value is passed in through the options object,
  // then we use it. Otherwise, we check if the numeric value is
  // passed in through the unicode extensions.
  status = U_ZERO_ERROR;
  if (found_numeric.FromJust()) {
    SetNumericOption(icu_collator.get(), numeric);
  } else {
    auto kn_extension_it = r.extensions.find("kn");
    if (kn_extension_it != r.extensions.end()) {
      SetNumericOption(icu_collator.get(), (kn_extension_it->second == "true"));
    }
  }

  // 23. If relevantExtensionKeys contains "kf", then
  //     a. Set collator.[[CaseFirst]] to r.[[kf]].
  //
  // If the caseFirst value is passed in through the options object,
  // then we use it. Otherwise, we check if the caseFirst value is
  // passed in through the unicode extensions.
  if (case_first != CaseFirst::kUndefined) {
    SetCaseFirstOption(icu_collator.get(), case_first);
  } else {
    auto kf_extension_it = r.extensions.find("kf");
    if (kf_extension_it != r.extensions.end()) {
      SetCaseFirstOption(icu_collator.get(),
                         ToCaseFirst(kf_extension_it->second.c_str()));
    }
  }

  // Normalization is always on, by the spec. We are free to optimize
  // if the strings are already normalized (but we don't have a way to tell
  // that right now).
  status = U_ZERO_ERROR;
  icu_collator->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);
  CHECK(U_SUCCESS(status));

  // 24. Let sensitivity be ? GetOption(options, "sensitivity",
  // "string", « "base", "accent", "case", "variant" », undefined).
  Maybe<Sensitivity> maybe_sensitivity = Intl::GetStringOption<Sensitivity>(
      isolate, options, "sensitivity", service,
      {"base", "accent", "case", "variant"},
      {Sensitivity::kBase, Sensitivity::kAccent, Sensitivity::kCase,
       Sensitivity::kVariant},
      Sensitivity::kUndefined);
  MAYBE_RETURN(maybe_sensitivity, MaybeHandle<JSCollator>());
  Sensitivity sensitivity = maybe_sensitivity.FromJust();

  // 25. If sensitivity is undefined, then
  if (sensitivity == Sensitivity::kUndefined) {
    // 25. a. If usage is "sort", then
    if (usage == Usage::SORT) {
      // 25. a. i. Let sensitivity be "variant".
      sensitivity = Sensitivity::kVariant;
    }
  }
  // 26. Set collator.[[Sensitivity]] to sensitivity.
  switch (sensitivity) {
    case Sensitivity::kBase:
      icu_collator->setStrength(icu::Collator::PRIMARY);
      break;
    case Sensitivity::kAccent:
      icu_collator->setStrength(icu::Collator::SECONDARY);
      break;
    case Sensitivity::kCase:
      icu_collator->setStrength(icu::Collator::PRIMARY);
      status = U_ZERO_ERROR;
      icu_collator->setAttribute(UCOL_CASE_LEVEL, UCOL_ON, status);
      CHECK(U_SUCCESS(status));
      break;
    case Sensitivity::kVariant:
      icu_collator->setStrength(icu::Collator::TERTIARY);
      break;
    case Sensitivity::kUndefined:
      break;
  }

  // 27.Let ignorePunctuation be ? GetOption(options,
  // "ignorePunctuation", "boolean", undefined, false).
  bool ignore_punctuation;
  Maybe<bool> found_ignore_punctuation = Intl::GetBoolOption(
      isolate, options, "ignorePunctuation", service, &ignore_punctuation);
  MAYBE_RETURN(found_ignore_punctuation, MaybeHandle<JSCollator>());

  // 28. Set collator.[[IgnorePunctuation]] to ignorePunctuation.
  if (found_ignore_punctuation.FromJust() && ignore_punctuation) {
    status = U_ZERO_ERROR;
    icu_collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, status);
    CHECK(U_SUCCESS(status));
  }

  Handle<Managed<icu::Collator>> managed_collator =
      Managed<icu::Collator>::FromUniquePtr(isolate, 0,
                                            std::move(icu_collator));

  // Now all properties are ready, so we can allocate the result object.
  Handle<JSCollator> collator = Handle<JSCollator>::cast(
      isolate->factory()->NewFastOrSlowJSObjectFromMap(map));
  DisallowHeapAllocation no_gc;
  collator->set_icu_collator(*managed_collator);

  // 29. Return collator.
  return collator;
}

namespace {

class CollatorAvailableLocales {
 public:
  CollatorAvailableLocales() {
    int32_t num_locales = 0;
    const icu::Locale* icu_available_locales =
        icu::Collator::getAvailableLocales(num_locales);
    std::vector<std::string> locales;
    for (int32_t i = 0; i < num_locales; ++i) {
      locales.push_back(
          Intl::ToLanguageTag(icu_available_locales[i]).FromJust());
    }
#define U_ICUDATA_COLL U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll"
    set_ = Intl::BuildLocaleSet(locales, U_ICUDATA_COLL, nullptr);
#undef U_ICUDATA_COLL
  }
  virtual ~CollatorAvailableLocales() {}
  const std::set<std::string>& Get() const { return set_; }

 private:
  std::set<std::string> set_;
};

}  // namespace

const std::set<std::string>& JSCollator::GetAvailableLocales() {
  static base::LazyInstance<CollatorAvailableLocales>::type available_locales =
      LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

}  // namespace internal
}  // namespace v8
