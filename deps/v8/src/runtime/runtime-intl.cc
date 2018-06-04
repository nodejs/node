// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/runtime/runtime-utils.h"

#include <cmath>
#include <memory>

#include "src/api-natives.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/intl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/objects/intl-objects.h"
#include "src/utils.h"

#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/datefmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/fieldpos.h"
#include "unicode/fpositer.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/translit.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"


namespace v8 {
namespace internal {

// ECMA 402 6.2.3
RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

  Factory* factory = isolate->factory();

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, locale_id_str, 0);

  v8::String::Utf8Value locale_id(v8_isolate,
                                  v8::Utils::ToLocal(locale_id_str));

  // TODO(jshin): uloc_{for,to}TanguageTag can fail even for a structually valid
  // language tag if it's too long (much longer than 100 chars). Even if we
  // allocate a longer buffer, ICU will still fail if it's too long. Either
  // propose to Ecma 402 to put a limit on the locale length or change ICU to
  // handle long locale names better. See
  // https://ssl.icu-project.org/trac/ticket/13417 .

  // Return value which denotes invalid language tag.
  const char* const kInvalidTag = "invalid-tag";

  UErrorCode error = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  uloc_forLanguageTag(*locale_id, icu_result, ULOC_FULLNAME_CAPACITY, nullptr,
                      &error);
  if (U_FAILURE(error) || error == U_STRING_NOT_TERMINATED_WARNING) {
    return *factory->NewStringFromAsciiChecked(kInvalidTag);
  }

  char result[ULOC_FULLNAME_CAPACITY];

  // Force strict BCP47 rules.
  uloc_toLanguageTag(icu_result, result, ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error) || error == U_STRING_NOT_TERMINATED_WARNING) {
    return *factory->NewStringFromAsciiChecked(kInvalidTag);
  }

  return *factory->NewStringFromAsciiChecked(result);
}

RUNTIME_FUNCTION(Runtime_AvailableLocalesOf) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);

  const icu::Locale* available_locales = nullptr;
  int32_t count = 0;

  if (service->IsUtf8EqualTo(CStrVector("collator"))) {
    available_locales = icu::Collator::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("numberformat"))) {
    available_locales = icu::NumberFormat::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("dateformat"))) {
    available_locales = icu::DateFormat::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("breakiterator"))) {
    available_locales = icu::BreakIterator::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("pluralrules"))) {
    // TODO(littledan): For PluralRules, filter out locales that
    // don't support PluralRules.
    // PluralRules is missing an appropriate getAvailableLocales method,
    // so we should filter from all locales, but it's not clear how; see
    // https://ssl.icu-project.org/trac/ticket/12756
    available_locales = icu::Locale::getAvailableLocales(count);
  } else {
    UNREACHABLE();
  }

  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];
  Handle<JSObject> locales = factory->NewJSObject(isolate->object_function());

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error) || error == U_STRING_NOT_TERMINATED_WARNING) {
      // This shouldn't happen, but lets not break the user.
      continue;
    }

    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                     locales, factory->NewStringFromAsciiChecked(result),
                     factory->NewNumber(i), NONE));
  }

  return *locales;
}

RUNTIME_FUNCTION(Runtime_GetDefaultICULocale) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK_EQ(0, args.length());

  icu::Locale default_locale;

  // Translate ICU's fallback locale to a well-known locale.
  if (strcmp(default_locale.getName(), "en_US_POSIX") == 0) {
    return *factory->NewStringFromStaticChars("en-US");
  }

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(default_locale.getName(), result, ULOC_FULLNAME_CAPACITY,
                     FALSE, &status);
  if (U_SUCCESS(status)) {
    return *factory->NewStringFromAsciiChecked(result);
  }

  return *factory->NewStringFromStaticChars("und");
}

RUNTIME_FUNCTION(Runtime_IsInitializedIntlObject) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  Handle<Object> tag = JSReceiver::GetDataProperty(obj, marker);
  return isolate->heap()->ToBoolean(!tag->IsUndefined(isolate));
}

RUNTIME_FUNCTION(Runtime_IsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, expected_type, 1);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  Handle<Object> tag = JSReceiver::GetDataProperty(obj, marker);
  return isolate->heap()->ToBoolean(tag->IsString() &&
                                    String::cast(*tag)->Equals(*expected_type));
}

RUNTIME_FUNCTION(Runtime_MarkAsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, type, 1);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  JSObject::SetProperty(input, marker, type, LanguageMode::kStrict).Assert();

  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_CreateDateTimeFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_date_time_format_function());

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set date time formatter as embedder field of the resulting JS object.
  icu::SimpleDateFormat* date_format =
      DateFormat::InitializeDateTimeFormat(isolate, locale, options, resolved);

  if (!date_format) return isolate->ThrowIllegalOperation();

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(date_format));

  // Make object handle weak so we can delete the data format once GC kicks in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          DateFormat::DeleteDateFormat,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_InternalDateFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(date, 1);

  double date_value = date->Number();
  // Check for +-Infinity and Nan
  if (!std::isfinite(date_value)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  CHECK_NOT_NULL(date_format);

  icu::UnicodeString result;
  date_format->format(date_value, result);

  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
                   reinterpret_cast<const uint16_t*>(result.getBuffer()),
                   result.length())));
}

namespace {
// The list comes from third_party/icu/source/i18n/unicode/udat.h.
// They're mapped to DateTimeFormat components listed at
// https://tc39.github.io/ecma402/#sec-datetimeformat-abstracts .

Handle<String> IcuDateFieldIdToDateType(int32_t field_id, Isolate* isolate) {
  switch (field_id) {
    case -1:
      return isolate->factory()->literal_string();
    case UDAT_YEAR_FIELD:
    case UDAT_EXTENDED_YEAR_FIELD:
    case UDAT_YEAR_NAME_FIELD:
      return isolate->factory()->year_string();
    case UDAT_MONTH_FIELD:
    case UDAT_STANDALONE_MONTH_FIELD:
      return isolate->factory()->month_string();
    case UDAT_DATE_FIELD:
      return isolate->factory()->day_string();
    case UDAT_HOUR_OF_DAY1_FIELD:
    case UDAT_HOUR_OF_DAY0_FIELD:
    case UDAT_HOUR1_FIELD:
    case UDAT_HOUR0_FIELD:
      return isolate->factory()->hour_string();
    case UDAT_MINUTE_FIELD:
      return isolate->factory()->minute_string();
    case UDAT_SECOND_FIELD:
      return isolate->factory()->second_string();
    case UDAT_DAY_OF_WEEK_FIELD:
    case UDAT_DOW_LOCAL_FIELD:
    case UDAT_STANDALONE_DAY_FIELD:
      return isolate->factory()->weekday_string();
    case UDAT_AM_PM_FIELD:
      return isolate->factory()->dayperiod_string();
    case UDAT_TIMEZONE_FIELD:
    case UDAT_TIMEZONE_RFC_FIELD:
    case UDAT_TIMEZONE_GENERIC_FIELD:
    case UDAT_TIMEZONE_SPECIAL_FIELD:
    case UDAT_TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD:
    case UDAT_TIMEZONE_ISO_FIELD:
    case UDAT_TIMEZONE_ISO_LOCAL_FIELD:
      return isolate->factory()->timeZoneName_string();
    case UDAT_ERA_FIELD:
      return isolate->factory()->era_string();
    default:
      // Other UDAT_*_FIELD's cannot show up because there is no way to specify
      // them via options of Intl.DateTimeFormat.
      UNREACHABLE();
      // To prevent MSVC from issuing C4715 warning.
      return Handle<String>();
  }
}

bool AddElement(Handle<JSArray> array, int index, int32_t field_id,
                const icu::UnicodeString& formatted, int32_t begin, int32_t end,
                Isolate* isolate) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();
  Handle<JSObject> element = factory->NewJSObject(isolate->object_function());
  Handle<String> value = IcuDateFieldIdToDateType(field_id, isolate);
  JSObject::AddProperty(element, factory->type_string(), value, NONE);

  icu::UnicodeString field(formatted.tempSubStringBetween(begin, end));
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      factory->NewStringFromTwoByte(Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(field.getBuffer()),
          field.length())),
      false);

  JSObject::AddProperty(element, factory->value_string(), value, NONE);
  RETURN_ON_EXCEPTION_VALUE(
      isolate, JSObject::AddDataElement(array, index, element, NONE), false);
  return true;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_InternalDateFormatToParts) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(date, 1);

  double date_value = date->Number();
  if (!std::isfinite(date_value)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  CHECK_NOT_NULL(date_format);

  icu::UnicodeString formatted;
  icu::FieldPositionIterator fp_iter;
  icu::FieldPosition fp;
  UErrorCode status = U_ZERO_ERROR;
  date_format->format(date_value, formatted, &fp_iter, status);
  if (U_FAILURE(status)) return isolate->heap()->undefined_value();

  Handle<JSArray> result = factory->NewJSArray(0);
  int32_t length = formatted.length();
  if (length == 0) return *result;

  int index = 0;
  int32_t previous_end_pos = 0;
  while (fp_iter.next(fp)) {
    int32_t begin_pos = fp.getBeginIndex();
    int32_t end_pos = fp.getEndIndex();

    if (previous_end_pos < begin_pos) {
      if (!AddElement(result, index, -1, formatted, previous_end_pos, begin_pos,
                      isolate)) {
        return isolate->heap()->undefined_value();
      }
      ++index;
    }
    if (!AddElement(result, index, fp.getField(), formatted, begin_pos, end_pos,
                    isolate)) {
      return isolate->heap()->undefined_value();
    }
    previous_end_pos = end_pos;
    ++index;
  }
  if (previous_end_pos < length) {
    if (!AddElement(result, index, -1, formatted, previous_end_pos, length,
                    isolate)) {
      return isolate->heap()->undefined_value();
    }
  }
  JSObject::ValidateElements(*result);
  return *result;
}

RUNTIME_FUNCTION(Runtime_CreateNumberFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_number_format_function());

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set number formatter as embedder field of the resulting JS object.
  icu::DecimalFormat* number_format =
      NumberFormat::InitializeNumberFormat(isolate, locale, options, resolved);

  if (!number_format) return isolate->ThrowIllegalOperation();

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(number_format));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          NumberFormat::DeleteNumberFormat,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_InternalNumberFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(number));

  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(isolate, number_format_holder);
  CHECK_NOT_NULL(number_format);

  icu::UnicodeString result;
  number_format->format(value->Number(), result);

  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
                   reinterpret_cast<const uint16_t*>(result.getBuffer()),
                   result.length())));
}

RUNTIME_FUNCTION(Runtime_CurrencyDigits) {
  DCHECK_EQ(1, args.length());
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

  CONVERT_ARG_HANDLE_CHECKED(String, currency, 0);

  v8::String::Value currency_string(v8_isolate, v8::Utils::ToLocal(currency));

  DisallowHeapAllocation no_gc;
  UErrorCode status = U_ZERO_ERROR;
  uint32_t fraction_digits = ucurr_getDefaultFractionDigits(
      reinterpret_cast<const UChar*>(*currency_string), &status);
  // For missing currency codes, default to the most common, 2
  if (!U_SUCCESS(status)) fraction_digits = 2;
  return Smi::FromInt(fraction_digits);
}

RUNTIME_FUNCTION(Runtime_CreateCollator) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_collator_function());

  Handle<JSObject> collator_holder;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, collator_holder,
                                     JSObject::New(constructor, constructor));

  if (!Collator::InitializeCollator(isolate, collator_holder, locale, options,
                                    resolved)) {
    return isolate->ThrowIllegalOperation();
  }

  return *collator_holder;
}

RUNTIME_FUNCTION(Runtime_InternalCompare) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, collator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, string1, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, string2, 2);

  icu::Collator* collator = Collator::UnpackCollator(isolate, collator_holder);
  CHECK_NOT_NULL(collator);

  string1 = String::Flatten(string1);
  string2 = String::Flatten(string2);

  UCollationResult result;
  UErrorCode status = U_ZERO_ERROR;
  {
    DisallowHeapAllocation no_gc;
    int32_t length1 = string1->length();
    int32_t length2 = string2->length();
    String::FlatContent flat1 = string1->GetFlatContent();
    String::FlatContent flat2 = string2->GetFlatContent();
    std::unique_ptr<uc16[]> sap1;
    std::unique_ptr<uc16[]> sap2;
    icu::UnicodeString string_val1(
        FALSE, GetUCharBufferFromFlat(flat1, &sap1, length1), length1);
    icu::UnicodeString string_val2(
        FALSE, GetUCharBufferFromFlat(flat2, &sap2, length2), length2);
    result = collator->compare(string_val1, string_val2, status);
  }
  if (U_FAILURE(status)) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(result);
}

RUNTIME_FUNCTION(Runtime_CreatePluralRules) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_plural_rules_function());

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set pluralRules as internal field of the resulting JS object.
  icu::PluralRules* plural_rules;
  icu::DecimalFormat* decimal_format;
  bool success = PluralRules::InitializePluralRules(
      isolate, locale, options, resolved, &plural_rules, &decimal_format);

  if (!success) return isolate->ThrowIllegalOperation();

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(plural_rules));
  local_object->SetEmbedderField(1, reinterpret_cast<Smi*>(decimal_format));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          PluralRules::DeletePluralRules,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_PluralRulesSelect) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, plural_rules_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  icu::PluralRules* plural_rules =
      PluralRules::UnpackPluralRules(isolate, plural_rules_holder);
  CHECK_NOT_NULL(plural_rules);

  icu::DecimalFormat* number_format =
      PluralRules::UnpackNumberFormat(isolate, plural_rules_holder);
  CHECK_NOT_NULL(number_format);

  // Currently, PluralRules doesn't implement all the options for rounding that
  // the Intl spec provides; format and parse the number to round to the
  // appropriate amount, then apply PluralRules.
  //
  // TODO(littledan): If a future ICU version supports an extended API to avoid
  // this step, then switch to that API. Bug thread:
  // http://bugs.icu-project.org/trac/ticket/12763
  icu::UnicodeString rounded_string;
  number_format->format(number->Number(), rounded_string);

  icu::Formattable formattable;
  UErrorCode status = U_ZERO_ERROR;
  number_format->parse(rounded_string, formattable, status);
  if (!U_SUCCESS(status)) return isolate->ThrowIllegalOperation();

  double rounded = formattable.getDouble(status);
  if (!U_SUCCESS(status)) return isolate->ThrowIllegalOperation();

  icu::UnicodeString result = plural_rules->select(rounded);
  return *isolate->factory()
              ->NewStringFromTwoByte(Vector<const uint16_t>(
                  reinterpret_cast<const uint16_t*>(result.getBuffer()),
                  result.length()))
              .ToHandleChecked();
}

RUNTIME_FUNCTION(Runtime_CreateBreakIterator) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_v8_break_iterator_function());

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set break iterator as embedder field of the resulting JS object.
  icu::BreakIterator* break_iterator = V8BreakIterator::InitializeBreakIterator(
      isolate, locale, options, resolved);

  if (!break_iterator) return isolate->ThrowIllegalOperation();

  local_object->SetEmbedderField(0, reinterpret_cast<Smi*>(break_iterator));
  // Make sure that the pointer to adopted text is nullptr.
  local_object->SetEmbedderField(1, static_cast<Smi*>(nullptr));

  // Make object handle weak so we can delete the break iterator once GC kicks
  // in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          V8BreakIterator::DeleteBreakIterator,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_BreakIteratorAdoptText) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, text, 1);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  icu::UnicodeString* u_text = reinterpret_cast<icu::UnicodeString*>(
      break_iterator_holder->GetEmbedderField(1));
  delete u_text;

  int length = text->length();
  text = String::Flatten(text);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = text->GetFlatContent();
  std::unique_ptr<uc16[]> sap;
  const UChar* text_value = GetUCharBufferFromFlat(flat, &sap, length);
  u_text = new icu::UnicodeString(text_value, length);
  break_iterator_holder->SetEmbedderField(1, reinterpret_cast<Smi*>(u_text));

  break_iterator->setText(*u_text);

  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_BreakIteratorFirst) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->first());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorNext) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->next());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorCurrent) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->current());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorBreakType) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("none");
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return isolate->heap()->number_string();
  } else if (status >= UBRK_WORD_LETTER && status < UBRK_WORD_LETTER_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("letter");
  } else if (status >= UBRK_WORD_KANA && status < UBRK_WORD_KANA_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("kana");
  } else if (status >= UBRK_WORD_IDEO && status < UBRK_WORD_IDEO_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("ideo");
  } else {
    return *isolate->factory()->NewStringFromStaticChars("unknown");
  }
}

RUNTIME_FUNCTION(Runtime_StringToLowerCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(s);
  return ConvertToLower(s, isolate);
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(s);
  return ConvertToUpper(s, isolate);
}

RUNTIME_FUNCTION(Runtime_StringLocaleConvertCase) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 3);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_upper, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, lang_arg, 2);

  // Primary language tag can be up to 8 characters long in theory.
  // https://tools.ietf.org/html/bcp47#section-2.2.1
  DCHECK_LE(lang_arg->length(), 8);
  lang_arg = String::Flatten(lang_arg);
  s = String::Flatten(s);

  // All the languages requiring special-handling have two-letter codes.
  // Note that we have to check for '!= 2' here because private-use language
  // tags (x-foo) or grandfathered irregular tags (e.g. i-enochian) would have
  // only 'x' or 'i' when they get here.
  if (V8_UNLIKELY(lang_arg->length() != 2))
    return ConvertCase(s, is_upper, isolate);

  char c1, c2;
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent lang = lang_arg->GetFlatContent();
    c1 = lang.Get(0);
    c2 = lang.Get(1);
  }
  // TODO(jshin): Consider adding a fast path for ASCII or Latin-1. The fastpath
  // in the root locale needs to be adjusted for az, lt and tr because even case
  // mapping of ASCII range characters are different in those locales.
  // Greek (el) does not require any adjustment.
  if (V8_UNLIKELY(c1 == 't' && c2 == 'r'))
    return LocaleConvertCase(s, isolate, is_upper, "tr");
  if (V8_UNLIKELY(c1 == 'e' && c2 == 'l'))
    return LocaleConvertCase(s, isolate, is_upper, "el");
  if (V8_UNLIKELY(c1 == 'l' && c2 == 't'))
    return LocaleConvertCase(s, isolate, is_upper, "lt");
  if (V8_UNLIKELY(c1 == 'a' && c2 == 'z'))
    return LocaleConvertCase(s, isolate, is_upper, "az");

  return ConvertCase(s, is_upper, isolate);
}

RUNTIME_FUNCTION(Runtime_DateCacheVersion) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  if (isolate->serializer_enabled()) return isolate->heap()->undefined_value();
  if (!isolate->eternal_handles()->Exists(EternalHandles::DATE_CACHE_VERSION)) {
    Handle<FixedArray> date_cache_version =
        isolate->factory()->NewFixedArray(1, TENURED);
    date_cache_version->set(0, Smi::kZero);
    isolate->eternal_handles()->CreateSingleton(
        isolate, *date_cache_version, EternalHandles::DATE_CACHE_VERSION);
  }
  Handle<FixedArray> date_cache_version =
      Handle<FixedArray>::cast(isolate->eternal_handles()->GetSingleton(
          EternalHandles::DATE_CACHE_VERSION));
  return date_cache_version->get(0);
}

}  // namespace internal
}  // namespace v8
