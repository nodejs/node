// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <memory>
#include <vector>

#include "src/objects/js-display-names-inl.h"
#include "src/objects/js-display-names.h"

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"

#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/localebuilder.h"
#include "unicode/locdspnm.h"
#include "unicode/timezone.h"
#include "unicode/tznames.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/uscript.h"

namespace v8 {
namespace internal {

namespace {
// Type: identifying the types of the display names.
//
// ecma402/#sec-properties-of-intl-displaynames-instances
enum class Type {
  kLanguage,
  kRegion,
  kScript,
  kCurrency,
  kWeekday,
  kMonth,
  kQuarter,
  kDayPeriod,
  kDateTimeField
};

bool IsUnicodeScriptSubtag(const std::string& value) {
  UErrorCode status = U_ZERO_ERROR;
  icu::LocaleBuilder builder;
  builder.setScript(value).build(status);
  return U_SUCCESS(status);
}

bool IsUnicodeRegionSubtag(const std::string& value) {
  UErrorCode status = U_ZERO_ERROR;
  icu::LocaleBuilder builder;
  builder.setRegion(value).build(status);
  return U_SUCCESS(status);
}

UDisplayContext ToUDisplayContext(JSDisplayNames::Style style) {
  switch (style) {
    case JSDisplayNames::Style::kLong:
      return UDISPCTX_LENGTH_FULL;
    case JSDisplayNames::Style::kShort:
    case JSDisplayNames::Style::kNarrow:
      return UDISPCTX_LENGTH_SHORT;
  }
}

}  // anonymous namespace

// Abstract class for all different types.
class DisplayNamesInternal {
 public:
  DisplayNamesInternal() = default;
  virtual ~DisplayNamesInternal() = default;
  virtual const char* type() const = 0;
  virtual icu::Locale locale() const = 0;
  virtual Maybe<icu::UnicodeString> of(Isolate* isolate,
                                       const char* code) const = 0;
  virtual const char* calendar() const { return nullptr; }
};

namespace {

class LocaleDisplayNamesCommon : public DisplayNamesInternal {
 public:
  LocaleDisplayNamesCommon(const icu::Locale& locale,
                           JSDisplayNames::Style style, bool fallback)
      : style_(style) {
    UDisplayContext sub =
        fallback ? UDISPCTX_SUBSTITUTE : UDISPCTX_NO_SUBSTITUTE;
    UDisplayContext display_context[] = {ToUDisplayContext(style_),
                                         UDISPCTX_DIALECT_NAMES,
                                         UDISPCTX_CAPITALIZATION_NONE, sub};
    ldn_.reset(
        icu::LocaleDisplayNames::createInstance(locale, display_context, 4));
  }

  ~LocaleDisplayNamesCommon() override = default;

  icu::Locale locale() const override { return ldn_->getLocale(); }

 protected:
  icu::LocaleDisplayNames* locale_display_names() const { return ldn_.get(); }

 private:
  std::unique_ptr<icu::LocaleDisplayNames> ldn_;
  JSDisplayNames::Style style_;
};

class LanguageNames : public LocaleDisplayNamesCommon {
 public:
  LanguageNames(const icu::Locale& locale, JSDisplayNames::Style style,
                bool fallback)
      : LocaleDisplayNamesCommon(locale, style, fallback) {}
  ~LanguageNames() override = default;
  const char* type() const override { return "language"; }
  Maybe<icu::UnicodeString> of(Isolate* isolate,
                               const char* code) const override {
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale l =
        icu::Locale(icu::Locale::forLanguageTag(code, status).getBaseName());
    std::string checked = l.toLanguageTag<std::string>(status);

    if (U_FAILURE(status)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kInvalidArgument),
          Nothing<icu::UnicodeString>());
    }

    icu::UnicodeString result;
    locale_display_names()->localeDisplayName(checked.c_str(), result);

    return Just(result);
  }
};

class RegionNames : public LocaleDisplayNamesCommon {
 public:
  RegionNames(const icu::Locale& locale, JSDisplayNames::Style style,
              bool fallback)
      : LocaleDisplayNamesCommon(locale, style, fallback) {}
  ~RegionNames() override = default;
  const char* type() const override { return "region"; }
  Maybe<icu::UnicodeString> of(Isolate* isolate,
                               const char* code) const override {
    std::string code_str(code);
    if (!IsUnicodeRegionSubtag(code_str)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kInvalidArgument),
          Nothing<icu::UnicodeString>());
    }

    icu::UnicodeString result;
    locale_display_names()->regionDisplayName(code_str.c_str(), result);
    return Just(result);
  }
};

class ScriptNames : public LocaleDisplayNamesCommon {
 public:
  ScriptNames(const icu::Locale& locale, JSDisplayNames::Style style,
              bool fallback)
      : LocaleDisplayNamesCommon(locale, style, fallback) {}
  ~ScriptNames() override = default;
  const char* type() const override { return "script"; }
  Maybe<icu::UnicodeString> of(Isolate* isolate,
                               const char* code) const override {
    std::string code_str(code);
    if (!IsUnicodeScriptSubtag(code_str)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kInvalidArgument),
          Nothing<icu::UnicodeString>());
    }

    icu::UnicodeString result;
    locale_display_names()->scriptDisplayName(code_str.c_str(), result);
    return Just(result);
  }
};

class CurrencyNames : public LocaleDisplayNamesCommon {
 public:
  CurrencyNames(const icu::Locale& locale, JSDisplayNames::Style style,
                bool fallback)
      : LocaleDisplayNamesCommon(locale, style, fallback) {}
  ~CurrencyNames() override = default;
  const char* type() const override { return "currency"; }
  Maybe<icu::UnicodeString> of(Isolate* isolate,
                               const char* code) const override {
    std::string code_str(code);
    if (!Intl::IsWellFormedCurrency(code_str)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kInvalidArgument),
          Nothing<icu::UnicodeString>());
    }

    icu::UnicodeString result;
    locale_display_names()->keyValueDisplayName("currency", code_str.c_str(),
                                                result);

    return Just(result);
  }
};

UDateTimePGDisplayWidth StyleToUDateTimePGDisplayWidth(
    JSDisplayNames::Style style) {
  switch (style) {
    case JSDisplayNames::Style::kLong:
      return UDATPG_WIDE;
    case JSDisplayNames::Style::kShort:
      return UDATPG_ABBREVIATED;
    case JSDisplayNames::Style::kNarrow:
      return UDATPG_NARROW;
  }
}

UDateTimePatternField StringToUDateTimePatternField(const char* code) {
  switch (code[0]) {
    case 'd':
      if (strcmp(code, "day") == 0) return UDATPG_DAY_FIELD;
      if (strcmp(code, "dayPeriod") == 0) return UDATPG_DAYPERIOD_FIELD;
      break;
    case 'e':
      if (strcmp(code, "era") == 0) return UDATPG_ERA_FIELD;
      break;
    case 'h':
      if (strcmp(code, "hour") == 0) return UDATPG_HOUR_FIELD;
      break;
    case 'm':
      if (strcmp(code, "minute") == 0) return UDATPG_MINUTE_FIELD;
      if (strcmp(code, "month") == 0) return UDATPG_MONTH_FIELD;
      break;
    case 'q':
      if (strcmp(code, "quarter") == 0) return UDATPG_QUARTER_FIELD;
      break;
    case 's':
      if (strcmp(code, "second") == 0) return UDATPG_SECOND_FIELD;
      break;
    case 't':
      if (strcmp(code, "timeZoneName") == 0) return UDATPG_ZONE_FIELD;
      break;
    case 'w':
      if (strcmp(code, "weekOfYear") == 0) return UDATPG_WEEK_OF_YEAR_FIELD;
      if (strcmp(code, "weekday") == 0) return UDATPG_WEEKDAY_FIELD;
      break;
    case 'y':
      if (strcmp(code, "year") == 0) return UDATPG_YEAR_FIELD;
      break;
    default:
      break;
  }
  UNREACHABLE();
}

class DateTimeFieldNames : public DisplayNamesInternal {
 public:
  DateTimeFieldNames(const icu::Locale& locale, JSDisplayNames::Style style)
      : locale_(locale), width_(StyleToUDateTimePGDisplayWidth(style)) {
    UErrorCode status = U_ZERO_ERROR;
    generator_.reset(
        icu::DateTimePatternGenerator::createInstance(locale_, status));
    CHECK(U_SUCCESS(status));
  }
  ~DateTimeFieldNames() override = default;
  const char* type() const override { return "dateTimeField"; }
  icu::Locale locale() const override { return locale_; }
  Maybe<icu::UnicodeString> of(Isolate* isolate,
                               const char* code) const override {
    UDateTimePatternField field = StringToUDateTimePatternField(code);
    if (field == UDATPG_FIELD_COUNT) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kInvalidArgument),
          Nothing<icu::UnicodeString>());
    }
    return Just(generator_->getFieldDisplayName(field, width_));
  }

 private:
  icu::Locale locale_;
  UDateTimePGDisplayWidth width_;
  std::unique_ptr<icu::DateTimePatternGenerator> generator_;
};

icu::DateFormatSymbols::DtWidthType StyleToDtWidthType(
    JSDisplayNames::Style style, Type type) {
  switch (style) {
    case JSDisplayNames::Style::kLong:
      return icu::DateFormatSymbols::WIDE;
    case JSDisplayNames::Style::kShort:
      return icu::DateFormatSymbols::SHORT;
    case JSDisplayNames::Style::kNarrow:
      if (type == Type::kQuarter) {
        return icu::DateFormatSymbols::ABBREVIATED;
      } else {
        return icu::DateFormatSymbols::NARROW;
      }
  }
}

class DateFormatSymbolsNames : public DisplayNamesInternal {
 public:
  DateFormatSymbolsNames(const char* type, const icu::Locale& locale,
                         const icu::UnicodeString* array, int32_t length,
                         const char* calendar)
      : type_(type),
        locale_(locale),
        array_(array),
        length_(length),
        calendar_(calendar) {}

  ~DateFormatSymbolsNames() override = default;

  const char* type() const override { return type_; }

  icu::Locale locale() const override { return locale_; }

  const char* calendar() const override {
    if (calendar_.empty()) {
      return nullptr;
    }
    return calendar_.c_str();
  }

  virtual int32_t ComputeIndex(const char* code) const = 0;

  Maybe<icu::UnicodeString> of(Isolate* isolate,
                               const char* code) const override {
    int32_t index = ComputeIndex(code);
    if (index < 0 || index >= length_) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kInvalidArgument),
          Nothing<icu::UnicodeString>());
    }
    return Just(array_[index]);
  }

 private:
  const char* type_;
  icu::Locale locale_;
  const icu::UnicodeString* array_;
  int32_t length_;
  std::string calendar_;
};

class WeekdayNames : public DateFormatSymbolsNames {
 public:
  WeekdayNames(const char* type, const icu::Locale& locale,
               const icu::UnicodeString* array, int32_t length,
               const char* calendar)
      : DateFormatSymbolsNames(type, locale, array, length, calendar) {}
  ~WeekdayNames() override = default;

  int32_t ComputeIndex(const char* code) const override {
    int32_t i = atoi(code);
    if (i == 7) return 1;
    if (i > 0 && i < 7) return i + 1;
    return -1;
  }
};

class MonthNames : public DateFormatSymbolsNames {
 public:
  MonthNames(const char* type, const icu::Locale& locale,
             const icu::UnicodeString* array, int32_t length,
             const char* calendar)
      : DateFormatSymbolsNames(type, locale, array, length, calendar) {}
  ~MonthNames() override = default;

  int32_t ComputeIndex(const char* code) const override {
    return atoi(code) - 1;
  }
};

class QuarterNames : public DateFormatSymbolsNames {
 public:
  QuarterNames(const char* type, const icu::Locale& locale,
               const icu::UnicodeString* array, int32_t length,
               const char* calendar)
      : DateFormatSymbolsNames(type, locale, array, length, calendar) {}
  ~QuarterNames() override = default;

  int32_t ComputeIndex(const char* code) const override {
    return atoi(code) - 1;
  }
};

class DayPeriodNames : public DateFormatSymbolsNames {
 public:
  DayPeriodNames(const char* type, const icu::Locale& locale,
                 const icu::UnicodeString* array, int32_t length,
                 const char* calendar)
      : DateFormatSymbolsNames(type, locale, array, length, calendar) {}
  ~DayPeriodNames() override = default;

  int32_t ComputeIndex(const char* code) const override {
    if (strcmp("am", code) == 0) {
      return 0;
    } else if (strcmp("pm", code) == 0) {
      return 1;
    } else {
      return -1;
    }
  }
};

const char* gWeekday = "weekday";
const char* gMonth = "month";
const char* gQuarter = "quarter";
const char* gDayPeriod = "dayPeriod";

DateFormatSymbolsNames* CreateDateFormatSymbolsNames(
    const icu::Locale& locale, JSDisplayNames::Style style, Type type) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::DateFormatSymbols> symbols(
      icu::DateFormatSymbols::createForLocale(locale, status));
  if (U_FAILURE(status)) {
    return nullptr;
  }
  icu::DateFormatSymbols::DtWidthType width_type =
      StyleToDtWidthType(style, type);
  int32_t count = 0;
  std::string calendar =
      locale.getUnicodeKeywordValue<std::string>("ca", status);

  switch (type) {
    case Type::kMonth:
      return new MonthNames(
          gMonth, locale,
          symbols->getMonths(count, icu::DateFormatSymbols::STANDALONE,
                             width_type),
          count, calendar.c_str());
    case Type::kWeekday:
      return new WeekdayNames(
          gWeekday, locale,
          symbols->getWeekdays(count, icu::DateFormatSymbols::STANDALONE,
                               width_type),
          count, calendar.c_str());
    case Type::kQuarter:
      return new QuarterNames(
          gQuarter, locale,
          symbols->getQuarters(count, icu::DateFormatSymbols::STANDALONE,
                               width_type),
          count, calendar.c_str());
    case Type::kDayPeriod:
      return new DayPeriodNames(gDayPeriod, locale,
                                symbols->getAmPmStrings(count), count,
                                calendar.c_str());
    default:
      UNREACHABLE();
  }
}

DisplayNamesInternal* CreateInternal(const icu::Locale& locale,
                                     JSDisplayNames::Style style, Type type,
                                     bool fallback) {
  switch (type) {
    case Type::kLanguage:
      return new LanguageNames(locale, style, fallback);
    case Type::kRegion:
      return new RegionNames(locale, style, fallback);
    case Type::kScript:
      return new ScriptNames(locale, style, fallback);
    case Type::kCurrency:
      return new CurrencyNames(locale, style, fallback);
    case Type::kDateTimeField:
      return new DateTimeFieldNames(locale, style);
    case Type::kMonth:
    case Type::kWeekday:
    case Type::kQuarter:
    case Type::kDayPeriod:
      return CreateDateFormatSymbolsNames(locale, style, type);
    default:
      UNREACHABLE();
  }
}

}  // anonymous namespace

// ecma402 #sec-Intl.DisplayNames
MaybeHandle<JSDisplayNames> JSDisplayNames::New(Isolate* isolate,
                                                Handle<Map> map,
                                                Handle<Object> locales,
                                                Handle<Object> input_options) {
  const char* service = "Intl.DisplayNames";
  Factory* factory = isolate->factory();

  Handle<JSReceiver> options;
  // 3. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSDisplayNames>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 4. If options is undefined, then
  if (input_options->IsUndefined(isolate)) {
    // 4. a. Let options be ObjectCreate(null).
    options = factory->NewJSObjectWithNullProto();
    // 5. Else
  } else {
    // 5. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               Object::ToObject(isolate, input_options),
                               JSDisplayNames);
  }

  // Note: No need to create a record. It's not observable.
  // 6. Let opt be a new Record.

  // 7. Let localeData be %DisplayNames%.[[LocaleData]].

  // 8. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.DisplayNames");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSDisplayNames>());

  // 9. Set opt.[[localeMatcher]] to matcher.
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  std::unique_ptr<char[]> calendar_str = nullptr;
  if (FLAG_harmony_intl_displaynames_date_types) {
    const std::vector<const char*> empty_values = {};
    // 10. Let calendar be ? GetOption(options, "calendar",
    //    "string", undefined, undefined).
    Maybe<bool> maybe_calendar = Intl::GetStringOption(
        isolate, options, "calendar", empty_values, service, &calendar_str);
    MAYBE_RETURN(maybe_calendar, MaybeHandle<JSDisplayNames>());
    // 11. If calendar is not undefined, then
    if (maybe_calendar.FromJust() && calendar_str != nullptr) {
      // a. If calendar does not match the (3*8alphanum) *("-" (3*8alphanum))
      //    sequence, throw a RangeError exception.
      if (!Intl::IsWellFormedCalendar(calendar_str.get())) {
        THROW_NEW_ERROR(
            isolate,
            NewRangeError(
                MessageTemplate::kInvalid, factory->calendar_string(),
                factory->NewStringFromAsciiChecked(calendar_str.get())),
            JSDisplayNames);
      }
    }
  }

  // 12. Set opt.[[ca]] to calendar.

  // ecma402/#sec-Intl.DisplayNames-internal-slots
  // The value of the [[RelevantExtensionKeys]] internal slot is
  // « "ca" ».
  std::set<std::string> relevant_extension_keys_ca = {"ca"};
  std::set<std::string> relevant_extension_keys = {};
  // 13. Let r be ResolveLocale(%DisplayNames%.[[AvailableLocales]],
  //     requestedLocales, opt, %DisplayNames%.[[RelevantExtensionKeys]]).
  Maybe<Intl::ResolvedLocale> maybe_resolve_locale = Intl::ResolveLocale(
      isolate, JSDisplayNames::GetAvailableLocales(), requested_locales,
      matcher,
      FLAG_harmony_intl_displaynames_date_types ? relevant_extension_keys_ca
                                                : relevant_extension_keys);
  if (maybe_resolve_locale.IsNothing()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError),
                    JSDisplayNames);
  }
  Intl::ResolvedLocale r = maybe_resolve_locale.FromJust();

  icu::Locale icu_locale = r.icu_locale;
  UErrorCode status = U_ZERO_ERROR;
  if (calendar_str != nullptr &&
      Intl::IsValidCalendar(icu_locale, calendar_str.get())) {
    icu_locale.setUnicodeKeywordValue("ca", calendar_str.get(), status);
    CHECK(U_SUCCESS(status));
  }

  // 14. Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  Maybe<Style> maybe_style = Intl::GetStringOption<Style>(
      isolate, options, "style", "Intl.DisplayNames",
      {"long", "short", "narrow"},
      {Style::kLong, Style::kShort, Style::kNarrow}, Style::kLong);
  MAYBE_RETURN(maybe_style, MaybeHandle<JSDisplayNames>());
  Style style_enum = maybe_style.FromJust();

  // 15. Set displayNames.[[Style]] to style.

  // 16. Let type be ? GetOption(options, "type", "string", « "language",
  //     "region", "script", "currency", "weekday", "month", "quarter",
  //     "dayPeriod", "dateTimeField" », "language").
  Maybe<Type> maybe_type =
      FLAG_harmony_intl_displaynames_date_types
          ? Intl::GetStringOption<Type>(
                isolate, options, "type", "Intl.DisplayNames",
                {"language", "region", "script", "currency", "weekday", "month",
                 "quarter", "dayPeriod", "dateTimeField"},
                {
                    Type::kLanguage,
                    Type::kRegion,
                    Type::kScript,
                    Type::kCurrency,
                    Type::kWeekday,
                    Type::kMonth,
                    Type::kQuarter,
                    Type::kDayPeriod,
                    Type::kDateTimeField,
                },
                Type::kLanguage)
          : Intl::GetStringOption<Type>(
                isolate, options, "type", "Intl.DisplayNames",
                {"language", "region", "script", "currency"},
                {
                    Type::kLanguage,
                    Type::kRegion,
                    Type::kScript,
                    Type::kCurrency,
                },
                Type::kLanguage);
  MAYBE_RETURN(maybe_type, MaybeHandle<JSDisplayNames>());
  Type type_enum = maybe_type.FromJust();

  // 17. Set displayNames.[[Type]] to type.

  // 18. Let fallback be ? GetOption(options, "fallback", "string",
  //     « "code", "none" », "code").
  Maybe<Fallback> maybe_fallback = Intl::GetStringOption<Fallback>(
      isolate, options, "fallback", "Intl.DisplayNames", {"code", "none"},
      {Fallback::kCode, Fallback::kNone}, Fallback::kCode);
  MAYBE_RETURN(maybe_fallback, MaybeHandle<JSDisplayNames>());
  Fallback fallback_enum = maybe_fallback.FromJust();

  // 19. Set displayNames.[[Fallback]] to fallback.

  // 20. Set displayNames.[[Locale]] to the value of r.[[Locale]].

  // Let calendar be r.[[ca]].

  // Set displayNames.[[Calendar]] to calendar.

  // Let dataLocale be r.[[dataLocale]].

  // Let dataLocaleData be localeData.[[<dataLocale>]].

  // Let types be dataLocaleData.[[types]].

  // Assert: types is a Record (see 1.3.3).

  // Let typeFields be types.[[<type>]].

  // Assert: typeFields is a Record (see 1.3.3).

  // Let styleFields be typeFields.[[<style>]].

  // Assert: styleFields is a Record (see 1.3.3).

  // Set displayNames.[[Fields]] to styleFields.

  DisplayNamesInternal* internal = CreateInternal(
      icu_locale, style_enum, type_enum, fallback_enum == Fallback::kCode);
  if (internal == nullptr) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError),
                    JSDisplayNames);
  }

  Handle<Managed<DisplayNamesInternal>> managed_internal =
      Managed<DisplayNamesInternal>::FromRawPtr(isolate, 0, internal);

  Handle<JSDisplayNames> display_names =
      Handle<JSDisplayNames>::cast(factory->NewFastOrSlowJSObjectFromMap(map));
  display_names->set_flags(0);
  display_names->set_style(style_enum);
  display_names->set_fallback(fallback_enum);

  DisallowHeapAllocation no_gc;
  display_names->set_internal(*managed_internal);

  // Return displayNames.
  return display_names;
}

// ecma402 #sec-Intl.DisplayNames.prototype.resolvedOptions
Handle<JSObject> JSDisplayNames::ResolvedOptions(
    Isolate* isolate, Handle<JSDisplayNames> display_names) {
  Factory* factory = isolate->factory();
  // 4. Let options be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> options = factory->NewJSObject(isolate->object_function());

  DisplayNamesInternal* internal = display_names->internal().raw();

  Maybe<std::string> maybe_locale = Intl::ToLanguageTag(internal->locale());
  CHECK(maybe_locale.IsJust());
  Handle<String> locale = isolate->factory()->NewStringFromAsciiChecked(
      maybe_locale.FromJust().c_str());
  Handle<String> style = display_names->StyleAsString();
  Handle<String> type = factory->NewStringFromAsciiChecked(internal->type());
  Handle<String> fallback = display_names->FallbackAsString();

  CHECK(JSReceiver::CreateDataProperty(isolate, options,
                                       factory->locale_string(), locale,
                                       Just(kDontThrow))
            .FromJust());
  if (internal->calendar() != nullptr) {
    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->calendar_string(),
              factory->NewStringFromAsciiChecked(internal->calendar()),
              Just(kDontThrow))
              .FromJust());
  }
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->style_string(), style, Just(kDontThrow))
            .FromJust());

  CHECK(JSReceiver::CreateDataProperty(isolate, options, factory->type_string(),
                                       type, Just(kDontThrow))
            .FromJust());
  CHECK(JSReceiver::CreateDataProperty(isolate, options,
                                       factory->fallback_string(), fallback,
                                       Just(kDontThrow))
            .FromJust());

  return options;
}

// ecma402 #sec-Intl.DisplayNames.prototype.of
MaybeHandle<Object> JSDisplayNames::Of(Isolate* isolate,
                                       Handle<JSDisplayNames> display_names,
                                       Handle<Object> code_obj) {
  Handle<String> code;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, code, Object::ToString(isolate, code_obj),
                             Object);
  DisplayNamesInternal* internal = display_names->internal().raw();
  Maybe<icu::UnicodeString> maybe_result =
      internal->of(isolate, code->ToCString().get());
  MAYBE_RETURN(maybe_result, Handle<Object>());
  icu::UnicodeString result = maybe_result.FromJust();
  if (result.isBogus()) {
    return isolate->factory()->undefined_value();
  }
  return Intl::ToString(isolate, result).ToHandleChecked();
}

namespace {

struct CheckCalendar {
  static const char* key() { return "calendar"; }
  static const char* path() { return nullptr; }
};

}  // namespace

const std::set<std::string>& JSDisplayNames::GetAvailableLocales() {
  static base::LazyInstance<Intl::AvailableLocales<CheckCalendar>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

Handle<String> JSDisplayNames::StyleAsString() const {
  switch (style()) {
    case Style::kLong:
      return GetReadOnlyRoots().long_string_handle();
    case Style::kShort:
      return GetReadOnlyRoots().short_string_handle();
    case Style::kNarrow:
      return GetReadOnlyRoots().narrow_string_handle();
  }
  UNREACHABLE();
}

Handle<String> JSDisplayNames::FallbackAsString() const {
  switch (fallback()) {
    case Fallback::kCode:
      return GetReadOnlyRoots().code_string_handle();
    case Fallback::kNone:
      return GetReadOnlyRoots().none_string_handle();
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
