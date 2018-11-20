// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifdef V8_I18N_SUPPORT
#include "src/v8.h"

#include "src/api-natives.h"
#include "src/arguments.h"
#include "src/i18n.h"
#include "src/runtime/runtime-utils.h"

#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/datefmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"


namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, locale_id_str, 0);

  v8::String::Utf8Value locale_id(v8::Utils::ToLocal(locale_id_str));

  // Return value which denotes invalid language tag.
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

  DCHECK(args.length() == 1);
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

  DCHECK(args.length() == 0);

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

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSArray, input, 0);

  uint32_t length = static_cast<uint32_t>(input->length()->Number());
  // Set some limit to prevent fuzz tests from going OOM.
  // Can be bumped when callers' requirements change.
  RUNTIME_ASSERT(length < 100);
  Handle<FixedArray> output = factory->NewFixedArray(length);
  Handle<Name> maximized = factory->NewStringFromStaticChars("maximized");
  Handle<Name> base = factory->NewStringFromStaticChars("base");
  for (unsigned int i = 0; i < length; ++i) {
    Handle<Object> locale_id;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, locale_id,
                                       Object::GetElement(isolate, input, i));
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

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  Handle<Object> tag = JSObject::GetDataProperty(obj, marker);
  return isolate->heap()->ToBoolean(!tag->IsUndefined());
}


RUNTIME_FUNCTION(Runtime_IsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, expected_type, 1);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  Handle<Object> tag = JSObject::GetDataProperty(obj, marker);
  return isolate->heap()->ToBoolean(tag->IsString() &&
                                    String::cast(*tag)->Equals(*expected_type));
}


RUNTIME_FUNCTION(Runtime_MarkAsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, type, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, impl, 2);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  JSObject::SetProperty(input, marker, type, STRICT).Assert();

  marker = isolate->factory()->intl_impl_object_symbol();
  JSObject::SetProperty(input, marker, impl, STRICT).Assert();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetImplFromInitializedIntlObject) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);

  if (!input->IsJSObject()) {
    Vector<Handle<Object> > arguments = HandleVector(&input, 1);
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError("not_intl_object", arguments));
  }

  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_impl_object_symbol();

  Handle<Object> impl = JSObject::GetDataProperty(obj, marker);
  if (impl->IsTheHole()) {
    Vector<Handle<Object> > arguments = HandleVector(&obj, 1);
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError("not_intl_object", arguments));
  }
  return *impl;
}


RUNTIME_FUNCTION(Runtime_CreateDateTimeFormat) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> date_format_template = I18N::GetTemplate(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object,
      ApiNatives::InstantiateObject(date_format_template));

  // Set date time formatter as internal field of the resulting JS object.
  icu::SimpleDateFormat* date_format =
      DateFormat::InitializeDateTimeFormat(isolate, locale, options, resolved);

  if (!date_format) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(date_format));

  Factory* factory = isolate->factory();
  Handle<String> key = factory->NewStringFromStaticChars("dateFormat");
  Handle<String> value = factory->NewStringFromStaticChars("valid");
  JSObject::AddProperty(local_object, key, value, NONE);

  // Make object handle weak so we can delete the data format once GC kicks in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          DateFormat::DeleteDateFormat);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalDateFormat) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     Execution::ToNumber(isolate, date));

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  if (!date_format) return isolate->ThrowIllegalOperation();

  icu::UnicodeString result;
  date_format->format(value->Number(), result);

  Handle<String> result_str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_str,
      isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(result.getBuffer()),
          result.length())));
  return *result_str;
}


RUNTIME_FUNCTION(Runtime_InternalDateParse) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, date_string, 1);

  v8::String::Utf8Value utf8_date(v8::Utils::ToLocal(date_string));
  icu::UnicodeString u_date(icu::UnicodeString::fromUTF8(*utf8_date));
  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  if (!date_format) return isolate->ThrowIllegalOperation();

  UErrorCode status = U_ZERO_ERROR;
  UDate date = date_format->parse(u_date, status);
  if (U_FAILURE(status)) return isolate->heap()->undefined_value();

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Execution::NewDate(isolate, static_cast<double>(date)));
  DCHECK(result->IsJSDate());
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateNumberFormat) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> number_format_template =
      I18N::GetTemplate(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object,
      ApiNatives::InstantiateObject(number_format_template));

  // Set number formatter as internal field of the resulting JS object.
  icu::DecimalFormat* number_format =
      NumberFormat::InitializeNumberFormat(isolate, locale, options, resolved);

  if (!number_format) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(number_format));

  Factory* factory = isolate->factory();
  Handle<String> key = factory->NewStringFromStaticChars("numberFormat");
  Handle<String> value = factory->NewStringFromStaticChars("valid");
  JSObject::AddProperty(local_object, key, value, NONE);

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          NumberFormat::DeleteNumberFormat);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalNumberFormat) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     Execution::ToNumber(isolate, number));

  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(isolate, number_format_holder);
  if (!number_format) return isolate->ThrowIllegalOperation();

  icu::UnicodeString result;
  number_format->format(value->Number(), result);

  Handle<String> result_str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_str,
      isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(result.getBuffer()),
          result.length())));
  return *result_str;
}


RUNTIME_FUNCTION(Runtime_InternalNumberParse) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, number_string, 1);

  v8::String::Utf8Value utf8_number(v8::Utils::ToLocal(number_string));
  icu::UnicodeString u_number(icu::UnicodeString::fromUTF8(*utf8_number));
  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(isolate, number_format_holder);
  if (!number_format) return isolate->ThrowIllegalOperation();

  UErrorCode status = U_ZERO_ERROR;
  icu::Formattable result;
  // ICU 4.6 doesn't support parseCurrency call. We need to wait for ICU49
  // to be part of Chrome.
  // TODO(cira): Include currency parsing code using parseCurrency call.
  // We need to check if the formatter parses all currencies or only the
  // one it was constructed with (it will impact the API - how to return ISO
  // code and the value).
  number_format->parse(u_number, result, status);
  if (U_FAILURE(status)) return isolate->heap()->undefined_value();

  switch (result.getType()) {
    case icu::Formattable::kDouble:
      return *isolate->factory()->NewNumber(result.getDouble());
    case icu::Formattable::kLong:
      return *isolate->factory()->NewNumberFromInt(result.getLong());
    case icu::Formattable::kInt64:
      return *isolate->factory()->NewNumber(
          static_cast<double>(result.getInt64()));
    default:
      return isolate->heap()->undefined_value();
  }
}


RUNTIME_FUNCTION(Runtime_CreateCollator) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> collator_template = I18N::GetTemplate(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object, ApiNatives::InstantiateObject(collator_template));

  // Set collator as internal field of the resulting JS object.
  icu::Collator* collator =
      Collator::InitializeCollator(isolate, locale, options, resolved);

  if (!collator) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(collator));

  Factory* factory = isolate->factory();
  Handle<String> key = factory->NewStringFromStaticChars("collator");
  Handle<String> value = factory->NewStringFromStaticChars("valid");
  JSObject::AddProperty(local_object, key, value, NONE);

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          Collator::DeleteCollator);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalCompare) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, collator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, string1, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, string2, 2);

  icu::Collator* collator = Collator::UnpackCollator(isolate, collator_holder);
  if (!collator) return isolate->ThrowIllegalOperation();

  v8::String::Value string_value1(v8::Utils::ToLocal(string1));
  v8::String::Value string_value2(v8::Utils::ToLocal(string2));
  const UChar* u_string1 = reinterpret_cast<const UChar*>(*string_value1);
  const UChar* u_string2 = reinterpret_cast<const UChar*>(*string_value2);
  UErrorCode status = U_ZERO_ERROR;
  UCollationResult result =
      collator->compare(u_string1, string_value1.length(), u_string2,
                        string_value2.length(), status);
  if (U_FAILURE(status)) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(result);
}


RUNTIME_FUNCTION(Runtime_StringNormalize) {
  HandleScope scope(isolate);
  static const UNormalizationMode normalizationForms[] = {
      UNORM_NFC, UNORM_NFD, UNORM_NFKC, UNORM_NFKD};

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, stringValue, 0);
  CONVERT_NUMBER_CHECKED(int, form_id, Int32, args[1]);
  RUNTIME_ASSERT(form_id >= 0 &&
                 static_cast<size_t>(form_id) < arraysize(normalizationForms));

  v8::String::Value string_value(v8::Utils::ToLocal(stringValue));
  const UChar* u_value = reinterpret_cast<const UChar*>(*string_value);

  // TODO(mnita): check Normalizer2 (not available in ICU 46)
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString result;
  icu::Normalizer::normalize(u_value, normalizationForms[form_id], 0, result,
                             status);
  if (U_FAILURE(status)) {
    return isolate->heap()->undefined_value();
  }

  Handle<String> result_str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_str,
      isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
          reinterpret_cast<const uint16_t*>(result.getBuffer()),
          result.length())));
  return *result_str;
}


RUNTIME_FUNCTION(Runtime_CreateBreakIterator) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> break_iterator_template =
      I18N::GetTemplate2(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object,
      ApiNatives::InstantiateObject(break_iterator_template));

  // Set break iterator as internal field of the resulting JS object.
  icu::BreakIterator* break_iterator = BreakIterator::InitializeBreakIterator(
      isolate, locale, options, resolved);

  if (!break_iterator) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(break_iterator));
  // Make sure that the pointer to adopted text is NULL.
  local_object->SetInternalField(1, reinterpret_cast<Smi*>(NULL));

  Factory* factory = isolate->factory();
  Handle<String> key = factory->NewStringFromStaticChars("breakIterator");
  Handle<String> value = factory->NewStringFromStaticChars("valid");
  JSObject::AddProperty(local_object, key, value, NONE);

  // Make object handle weak so we can delete the break iterator once GC kicks
  // in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          BreakIterator::DeleteBreakIterator);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_BreakIteratorAdoptText) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, text, 1);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  icu::UnicodeString* u_text = reinterpret_cast<icu::UnicodeString*>(
      break_iterator_holder->GetInternalField(1));
  delete u_text;

  v8::String::Value text_value(v8::Utils::ToLocal(text));
  u_text = new icu::UnicodeString(reinterpret_cast<const UChar*>(*text_value),
                                  text_value.length());
  break_iterator_holder->SetInternalField(1, reinterpret_cast<Smi*>(u_text));

  break_iterator->setText(*u_text);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_BreakIteratorFirst) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(break_iterator->first());
}


RUNTIME_FUNCTION(Runtime_BreakIteratorNext) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(break_iterator->next());
}


RUNTIME_FUNCTION(Runtime_BreakIteratorCurrent) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(break_iterator->current());
}


RUNTIME_FUNCTION(Runtime_BreakIteratorBreakType) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("none");
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return *isolate->factory()->number_string();
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
}
}  // namespace v8::internal

#endif  // V8_I18N_SUPPORT
