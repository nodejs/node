// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifdef V8_I18N_SUPPORT
#include "src/runtime/runtime-utils.h"

#include <memory>

#include "src/api-natives.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/factory.h"
#include "src/i18n.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/string-case.h"
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
#include "unicode/normalizer2.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
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
namespace {

const UChar* GetUCharBufferFromFlat(const String::FlatContent& flat,
                                    std::unique_ptr<uc16[]>* dest,
                                    int32_t length) {
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    if (!*dest) {
      dest->reset(NewArray<uc16>(length));
      CopyChars(dest->get(), flat.ToOneByteVector().start(), length);
    }
    return reinterpret_cast<const UChar*>(dest->get());
  } else {
    return reinterpret_cast<const UChar*>(flat.ToUC16Vector().start());
  }
}

}  // namespace

// ECMA 402 6.2.3
RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, locale_id_str, 0);

  v8::String::Utf8Value locale_id(v8::Utils::ToLocal(locale_id_str));

  // Return value which denotes invalid language tag.
  // TODO(jshin): Can uloc_{for,to}TanguageTag fail even for structually valid
  // language tags? If not, just add CHECK instead of returning 'invalid-tag'.
  const char* const kInvalidTag = "invalid-tag";

  UErrorCode error = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;

  uloc_forLanguageTag(*locale_id, icu_result, ULOC_FULLNAME_CAPACITY,
                      &icu_length, &error);
  if (U_FAILURE(error) || icu_length == 0) {
    return *factory->NewStringFromAsciiChecked(kInvalidTag);
  }

  char result[ULOC_FULLNAME_CAPACITY];

  // Force strict BCP47 rules.
  uloc_toLanguageTag(icu_result, result, ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error)) {
    return *factory->NewStringFromAsciiChecked(kInvalidTag);
  }

  return *factory->NewStringFromAsciiChecked(result);
}


RUNTIME_FUNCTION(Runtime_AvailableLocalesOf) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);

  const icu::Locale* available_locales = NULL;
  int32_t count = 0;

  if (service->IsUtf8EqualTo(CStrVector("collator"))) {
    available_locales = icu::Collator::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("numberformat"))) {
    available_locales = icu::NumberFormat::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("dateformat"))) {
    available_locales = icu::DateFormat::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("breakiterator"))) {
    available_locales = icu::BreakIterator::getAvailableLocales(count);
  }

  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];
  Handle<JSObject> locales = factory->NewJSObject(isolate->object_function());

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error)) {
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


RUNTIME_FUNCTION(Runtime_GetLanguageTagVariants) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSArray, input, 0);

  uint32_t length = static_cast<uint32_t>(input->length()->Number());
  // Set some limit to prevent fuzz tests from going OOM.
  // Can be bumped when callers' requirements change.
  if (length >= 100) return isolate->ThrowIllegalOperation();
  Handle<FixedArray> output = factory->NewFixedArray(length);
  Handle<Name> maximized = factory->NewStringFromStaticChars("maximized");
  Handle<Name> base = factory->NewStringFromStaticChars("base");
  for (unsigned int i = 0; i < length; ++i) {
    Handle<Object> locale_id;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, locale_id, JSReceiver::GetElement(isolate, input, i));
    if (!locale_id->IsString()) {
      return isolate->Throw(*factory->illegal_argument_string());
    }

    v8::String::Utf8Value utf8_locale_id(
        v8::Utils::ToLocal(Handle<String>::cast(locale_id)));

    UErrorCode error = U_ZERO_ERROR;

    // Convert from BCP47 to ICU format.
    // de-DE-u-co-phonebk -> de_DE@collation=phonebook
    char icu_locale[ULOC_FULLNAME_CAPACITY];
    int icu_locale_length = 0;
    uloc_forLanguageTag(*utf8_locale_id, icu_locale, ULOC_FULLNAME_CAPACITY,
                        &icu_locale_length, &error);
    if (U_FAILURE(error) || icu_locale_length == 0) {
      return isolate->Throw(*factory->illegal_argument_string());
    }

    // Maximize the locale.
    // de_DE@collation=phonebook -> de_Latn_DE@collation=phonebook
    char icu_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_addLikelySubtags(icu_locale, icu_max_locale, ULOC_FULLNAME_CAPACITY,
                          &error);

    // Remove extensions from maximized locale.
    // de_Latn_DE@collation=phonebook -> de_Latn_DE
    char icu_base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(icu_max_locale, icu_base_max_locale,
                     ULOC_FULLNAME_CAPACITY, &error);

    // Get original name without extensions.
    // de_DE@collation=phonebook -> de_DE
    char icu_base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(icu_locale, icu_base_locale, ULOC_FULLNAME_CAPACITY,
                     &error);

    // Convert from ICU locale format to BCP47 format.
    // de_Latn_DE -> de-Latn-DE
    char base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(icu_base_max_locale, base_max_locale,
                       ULOC_FULLNAME_CAPACITY, FALSE, &error);

    // de_DE -> de-DE
    char base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(icu_base_locale, base_locale, ULOC_FULLNAME_CAPACITY,
                       FALSE, &error);

    if (U_FAILURE(error)) {
      return isolate->Throw(*factory->illegal_argument_string());
    }

    Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
    Handle<String> value = factory->NewStringFromAsciiChecked(base_max_locale);
    JSObject::AddProperty(result, maximized, value, NONE);
    value = factory->NewStringFromAsciiChecked(base_locale);
    JSObject::AddProperty(result, base, value, NONE);
    output->set(i, *result);
  }

  Handle<JSArray> result = factory->NewJSArrayWithElements(output);
  result->set_length(Smi::FromInt(length));
  return *result;
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
  JSObject::SetProperty(input, marker, type, STRICT).Assert();

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

  // Set date time formatter as internal field of the resulting JS object.
  icu::SimpleDateFormat* date_format =
      DateFormat::InitializeDateTimeFormat(isolate, locale, options, resolved);

  if (!date_format) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(date_format));

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
  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(date));

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  CHECK_NOT_NULL(date_format);

  icu::UnicodeString result;
  date_format->format(value->Number(), result);

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
      isolate, value, factory->NewStringFromTwoByte(Vector<const uint16_t>(
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
  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(date));

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  CHECK_NOT_NULL(date_format);

  icu::UnicodeString formatted;
  icu::FieldPositionIterator fp_iter;
  icu::FieldPosition fp;
  UErrorCode status = U_ZERO_ERROR;
  date_format->format(value->Number(), formatted, &fp_iter, status);
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
  JSObject::ValidateElements(result);
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

  // Set number formatter as internal field of the resulting JS object.
  icu::DecimalFormat* number_format =
      NumberFormat::InitializeNumberFormat(isolate, locale, options, resolved);

  if (!number_format) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(number_format));

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


RUNTIME_FUNCTION(Runtime_CreateCollator) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_collator_function());

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set collator as internal field of the resulting JS object.
  icu::Collator* collator =
      Collator::InitializeCollator(isolate, locale, options, resolved);

  if (!collator) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(collator));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          Collator::DeleteCollator,
                          WeakCallbackType::kInternalFields);
  return *local_object;
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
    const UChar* string_val1 = GetUCharBufferFromFlat(flat1, &sap1, length1);
    const UChar* string_val2 = GetUCharBufferFromFlat(flat2, &sap2, length2);
    result =
        collator->compare(string_val1, length1, string_val2, length2, status);
  }
  if (U_FAILURE(status)) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(result);
}


RUNTIME_FUNCTION(Runtime_StringNormalize) {
  HandleScope scope(isolate);
  static const struct {
    const char* name;
    UNormalization2Mode mode;
  } normalizationForms[] = {
      {"nfc", UNORM2_COMPOSE},
      {"nfc", UNORM2_DECOMPOSE},
      {"nfkc", UNORM2_COMPOSE},
      {"nfkc", UNORM2_DECOMPOSE},
  };

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_NUMBER_CHECKED(int, form_id, Int32, args[1]);
  CHECK(form_id >= 0 &&
        static_cast<size_t>(form_id) < arraysize(normalizationForms));

  int length = s->length();
  s = String::Flatten(s);
  icu::UnicodeString result;
  std::unique_ptr<uc16[]> sap;
  UErrorCode status = U_ZERO_ERROR;
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = s->GetFlatContent();
    const UChar* src = GetUCharBufferFromFlat(flat, &sap, length);
    icu::UnicodeString input(false, src, length);
    // Getting a singleton. Should not free it.
    const icu::Normalizer2* normalizer =
        icu::Normalizer2::getInstance(nullptr, normalizationForms[form_id].name,
                                      normalizationForms[form_id].mode, status);
    DCHECK(U_SUCCESS(status));
    CHECK(normalizer != nullptr);
    int32_t normalized_prefix_length =
        normalizer->spanQuickCheckYes(input, status);
    // Quick return if the input is already normalized.
    if (length == normalized_prefix_length) return *s;
    icu::UnicodeString unnormalized =
        input.tempSubString(normalized_prefix_length);
    // Read-only alias of the normalized prefix.
    result.setTo(false, input.getBuffer(), normalized_prefix_length);
    // copy-on-write; normalize the suffix and append to |result|.
    normalizer->normalizeSecondAndAppend(result, unnormalized, status);
  }

  if (U_FAILURE(status)) {
    return isolate->heap()->undefined_value();
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
                   reinterpret_cast<const uint16_t*>(result.getBuffer()),
                   result.length())));
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

  // Set break iterator as internal field of the resulting JS object.
  icu::BreakIterator* break_iterator = V8BreakIterator::InitializeBreakIterator(
      isolate, locale, options, resolved);

  if (!break_iterator) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(break_iterator));
  // Make sure that the pointer to adopted text is NULL.
  local_object->SetInternalField(1, static_cast<Smi*>(nullptr));

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
      break_iterator_holder->GetInternalField(1));
  delete u_text;

  int length = text->length();
  text = String::Flatten(text);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = text->GetFlatContent();
  std::unique_ptr<uc16[]> sap;
  const UChar* text_value = GetUCharBufferFromFlat(flat, &sap, length);
  u_text = new icu::UnicodeString(text_value, length);
  break_iterator_holder->SetInternalField(1, reinterpret_cast<Smi*>(u_text));

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

namespace {
MUST_USE_RESULT Object* LocaleConvertCase(Handle<String> s, Isolate* isolate,
                                          bool is_to_upper, const char* lang) {
  auto case_converter = is_to_upper ? u_strToUpper : u_strToLower;
  int32_t src_length = s->length();
  int32_t dest_length = src_length;
  UErrorCode status;
  Handle<SeqTwoByteString> result;
  std::unique_ptr<uc16[]> sap;

  // This is not a real loop. It'll be executed only once (no overflow) or
  // twice (overflow).
  for (int i = 0; i < 2; ++i) {
    // Case conversion can increase the string length (e.g. sharp-S => SS) so
    // that we have to handle RangeError exceptions here.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(dest_length));
    DisallowHeapAllocation no_gc;
    DCHECK(s->IsFlat());
    String::FlatContent flat = s->GetFlatContent();
    const UChar* src = GetUCharBufferFromFlat(flat, &sap, src_length);
    status = U_ZERO_ERROR;
    dest_length = case_converter(reinterpret_cast<UChar*>(result->GetChars()),
                                 dest_length, src, src_length, lang, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) break;
  }

  // In most cases, the output will fill the destination buffer completely
  // leading to an unterminated string (U_STRING_NOT_TERMINATED_WARNING).
  // Only in rare cases, it'll be shorter than the destination buffer and
  // |result| has to be truncated.
  DCHECK(U_SUCCESS(status));
  if (V8_LIKELY(status == U_STRING_NOT_TERMINATED_WARNING)) {
    DCHECK(dest_length == result->length());
    return *result;
  }
  if (U_SUCCESS(status)) {
    DCHECK(dest_length < result->length());
    return *Handle<SeqTwoByteString>::cast(
        SeqString::Truncate(result, dest_length));
  }
  return *s;
}

inline bool IsASCIIUpper(uint16_t ch) { return ch >= 'A' && ch <= 'Z'; }

const uint8_t kToLower[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
    0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,
    0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB,
    0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xD7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
    0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB,
    0xFC, 0xFD, 0xFE, 0xFF,
};

inline uint16_t ToLatin1Lower(uint16_t ch) {
  return static_cast<uint16_t>(kToLower[ch]);
}

inline uint16_t ToASCIIUpper(uint16_t ch) {
  return ch & ~((ch >= 'a' && ch <= 'z') << 5);
}

// Does not work for U+00DF (sharp-s), U+00B5 (micron), U+00FF.
inline uint16_t ToLatin1Upper(uint16_t ch) {
  DCHECK(ch != 0xDF && ch != 0xB5 && ch != 0xFF);
  return ch &
         ~(((ch >= 'a' && ch <= 'z') || (((ch & 0xE0) == 0xE0) && ch != 0xF7))
           << 5);
}

template <typename Char>
bool ToUpperFastASCII(const Vector<const Char>& src,
                      Handle<SeqOneByteString> result) {
  // Do a faster loop for the case where all the characters are ASCII.
  uint16_t ored = 0;
  int32_t index = 0;
  for (auto it = src.begin(); it != src.end(); ++it) {
    uint16_t ch = static_cast<uint16_t>(*it);
    ored |= ch;
    result->SeqOneByteStringSet(index++, ToASCIIUpper(ch));
  }
  return !(ored & ~0x7F);
}

const uint16_t sharp_s = 0xDF;

template <typename Char>
bool ToUpperOneByte(const Vector<const Char>& src, uint8_t* dest,
                    int* sharp_s_count) {
  // Still pretty-fast path for the input with non-ASCII Latin-1 characters.

  // There are two special cases.
  //  1. U+00B5 and U+00FF are mapped to a character beyond U+00FF.
  //  2. Lower case sharp-S converts to "SS" (two characters)
  *sharp_s_count = 0;
  for (auto it = src.begin(); it != src.end(); ++it) {
    uint16_t ch = static_cast<uint16_t>(*it);
    if (V8_UNLIKELY(ch == sharp_s)) {
      ++(*sharp_s_count);
      continue;
    }
    if (V8_UNLIKELY(ch == 0xB5 || ch == 0xFF)) {
      // Since this upper-cased character does not fit in an 8-bit string, we
      // need to take the 16-bit path.
      return false;
    }
    *dest++ = ToLatin1Upper(ch);
  }

  return true;
}

template <typename Char>
void ToUpperWithSharpS(const Vector<const Char>& src,
                       Handle<SeqOneByteString> result) {
  int32_t dest_index = 0;
  for (auto it = src.begin(); it != src.end(); ++it) {
    uint16_t ch = static_cast<uint16_t>(*it);
    if (ch == sharp_s) {
      result->SeqOneByteStringSet(dest_index++, 'S');
      result->SeqOneByteStringSet(dest_index++, 'S');
    } else {
      result->SeqOneByteStringSet(dest_index++, ToLatin1Upper(ch));
    }
  }
}

inline int FindFirstUpperOrNonAscii(Handle<String> s, int length) {
  for (int index = 0; index < length; ++index) {
    uint16_t ch = s->Get(index);
    if (V8_UNLIKELY(IsASCIIUpper(ch) || ch & ~0x7F)) {
      return index;
    }
  }
  return length;
}

MUST_USE_RESULT Object* ConvertToLower(Handle<String> s, Isolate* isolate) {
  if (!s->HasOnlyOneByteChars()) {
    // Use a slower implementation for strings with characters beyond U+00FF.
    return LocaleConvertCase(s, isolate, false, "");
  }

  int length = s->length();

  // We depend here on the invariant that the length of a Latin1
  // string is invariant under ToLowerCase, and the result always
  // fits in the Latin1 range in the *root locale*. It does not hold
  // for ToUpperCase even in the root locale.

  // Scan the string for uppercase and non-ASCII characters for strings
  // shorter than a machine-word without any memory allocation overhead.
  // TODO(jshin): Apply this to a longer input by breaking FastAsciiConvert()
  // to two parts, one for scanning the prefix with no change and the other for
  // handling ASCII-only characters.
  int index_to_first_unprocessed = length;
  const bool is_short = length < static_cast<int>(sizeof(uintptr_t));
  if (is_short) {
    index_to_first_unprocessed = FindFirstUpperOrNonAscii(s, length);
    // Nothing to do if the string is all ASCII with no uppercase.
    if (index_to_first_unprocessed == length) return *s;
  }

  Handle<SeqOneByteString> result =
      isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

  DisallowHeapAllocation no_gc;
  DCHECK(s->IsFlat());
  String::FlatContent flat = s->GetFlatContent();
  uint8_t* dest = result->GetChars();
  if (flat.IsOneByte()) {
    const uint8_t* src = flat.ToOneByteVector().start();
    bool has_changed_character = false;
    index_to_first_unprocessed = FastAsciiConvert<true>(
        reinterpret_cast<char*>(dest), reinterpret_cast<const char*>(src),
        length, &has_changed_character);
    // If not ASCII, we keep the result up to index_to_first_unprocessed and
    // process the rest.
    if (index_to_first_unprocessed == length)
      return has_changed_character ? *result : *s;

    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dest[index] = ToLatin1Lower(static_cast<uint16_t>(src[index]));
    }
  } else {
    if (index_to_first_unprocessed == length) {
      DCHECK(!is_short);
      index_to_first_unprocessed = FindFirstUpperOrNonAscii(s, length);
    }
    // Nothing to do if the string is all ASCII with no uppercase.
    if (index_to_first_unprocessed == length) return *s;
    const uint16_t* src = flat.ToUC16Vector().start();
    CopyChars(dest, src, index_to_first_unprocessed);
    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dest[index] = ToLatin1Lower(static_cast<uint16_t>(src[index]));
    }
  }

  return *result;
}

MUST_USE_RESULT Object* ConvertToUpper(Handle<String> s, Isolate* isolate) {
  int32_t length = s->length();
  if (s->HasOnlyOneByteChars()) {
    Handle<SeqOneByteString> result =
        isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

    DCHECK(s->IsFlat());
    int sharp_s_count;
    bool is_result_single_byte;
    {
      DisallowHeapAllocation no_gc;
      String::FlatContent flat = s->GetFlatContent();
      uint8_t* dest = result->GetChars();
      if (flat.IsOneByte()) {
        Vector<const uint8_t> src = flat.ToOneByteVector();
        bool has_changed_character = false;
        int index_to_first_unprocessed =
            FastAsciiConvert<false>(reinterpret_cast<char*>(result->GetChars()),
                                    reinterpret_cast<const char*>(src.start()),
                                    length, &has_changed_character);
        if (index_to_first_unprocessed == length)
          return has_changed_character ? *result : *s;
        // If not ASCII, we keep the result up to index_to_first_unprocessed and
        // process the rest.
        is_result_single_byte =
            ToUpperOneByte(src.SubVector(index_to_first_unprocessed, length),
                           dest + index_to_first_unprocessed, &sharp_s_count);
      } else {
        DCHECK(flat.IsTwoByte());
        Vector<const uint16_t> src = flat.ToUC16Vector();
        if (ToUpperFastASCII(src, result)) return *result;
        is_result_single_byte = ToUpperOneByte(src, dest, &sharp_s_count);
      }
    }

    // Go to the full Unicode path if there are characters whose uppercase
    // is beyond the Latin-1 range (cannot be represented in OneByteString).
    if (V8_UNLIKELY(!is_result_single_byte)) {
      return LocaleConvertCase(s, isolate, true, "");
    }

    if (sharp_s_count == 0) return *result;

    // We have sharp_s_count sharp-s characters, but the result is still
    // in the Latin-1 range.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        isolate->factory()->NewRawOneByteString(length + sharp_s_count));
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = s->GetFlatContent();
    if (flat.IsOneByte()) {
      ToUpperWithSharpS(flat.ToOneByteVector(), result);
    } else {
      ToUpperWithSharpS(flat.ToUC16Vector(), result);
    }

    return *result;
  }

  return LocaleConvertCase(s, isolate, true, "");
}

MUST_USE_RESULT Object* ConvertCase(Handle<String> s, bool is_upper,
                                    Isolate* isolate) {
  return is_upper ? ConvertToUpper(s, isolate) : ConvertToLower(s, isolate);
}

}  // namespace

RUNTIME_FUNCTION(Runtime_StringToLowerCaseI18N) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(s);
  return ConvertToLower(s, isolate);
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseI18N) {
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
  DCHECK(lang_arg->length() <= 8);
  lang_arg = String::Flatten(lang_arg);
  s = String::Flatten(s);

  // All the languages requiring special-handling have two-letter codes.
  if (V8_UNLIKELY(lang_arg->length() > 2))
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

#endif  // V8_I18N_SUPPORT
