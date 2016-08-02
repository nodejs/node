// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifdef V8_I18N_SUPPORT
#include "src/runtime/runtime-utils.h"

#include "src/api.h"
#include "src/api-natives.h"
#include "src/arguments.h"
#include "src/factory.h"
#include "src/i18n.h"
#include "src/isolate-inl.h"
#include "src/messages.h"

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
                                    base::SmartArrayPointer<uc16>* dest,
                                    int32_t length) {
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    if (dest->is_empty()) {
      dest->Reset(NewArray<uc16>(length));
      CopyChars(dest->get(), flat.ToOneByteVector().start(), length);
    }
    return reinterpret_cast<const UChar*>(dest->get());
  } else {
    return reinterpret_cast<const UChar*>(flat.ToUC16Vector().start());
  }
}

}  // namespace

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

  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  Handle<Object> tag = JSReceiver::GetDataProperty(obj, marker);
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
  Handle<Object> tag = JSReceiver::GetDataProperty(obj, marker);
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

  CONVERT_ARG_HANDLE_CHECKED(JSObject, input, 0);

  if (!input->IsJSObject()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotIntlObject, input));
  }

  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_impl_object_symbol();

  Handle<Object> impl = JSReceiver::GetDataProperty(obj, marker);
  if (impl->IsTheHole()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotIntlObject, obj));
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
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          DateFormat::DeleteDateFormat,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalDateFormat) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(date));

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

  Handle<JSDate> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSDate::New(isolate->date_function(), isolate->date_function(),
                  static_cast<double>(date)));
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
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          NumberFormat::DeleteNumberFormat,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalNumberFormat) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value, Object::ToNumber(number));

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

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kIntlV8Parse);

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
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          Collator::DeleteCollator,
                          WeakCallbackType::kInternalFields);
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

  string1 = String::Flatten(string1);
  string2 = String::Flatten(string2);
  DisallowHeapAllocation no_gc;
  int32_t length1 = string1->length();
  int32_t length2 = string2->length();
  String::FlatContent flat1 = string1->GetFlatContent();
  String::FlatContent flat2 = string2->GetFlatContent();
  base::SmartArrayPointer<uc16> sap1;
  base::SmartArrayPointer<uc16> sap2;
  const UChar* string_val1 = GetUCharBufferFromFlat(flat1, &sap1, length1);
  const UChar* string_val2 = GetUCharBufferFromFlat(flat2, &sap2, length2);
  UErrorCode status = U_ZERO_ERROR;
  UCollationResult result =
      collator->compare(string_val1, length1, string_val2, length2, status);
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

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_NUMBER_CHECKED(int, form_id, Int32, args[1]);
  RUNTIME_ASSERT(form_id >= 0 &&
                 static_cast<size_t>(form_id) < arraysize(normalizationForms));

  int length = s->length();
  s = String::Flatten(s);
  icu::UnicodeString result;
  base::SmartArrayPointer<uc16> sap;
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
    RUNTIME_ASSERT(normalizer != nullptr);
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
  local_object->SetInternalField(1, static_cast<Smi*>(nullptr));

  Factory* factory = isolate->factory();
  Handle<String> key = factory->NewStringFromStaticChars("breakIterator");
  Handle<String> value = factory->NewStringFromStaticChars("valid");
  JSObject::AddProperty(local_object, key, value, NONE);

  // Make object handle weak so we can delete the break iterator once GC kicks
  // in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          BreakIterator::DeleteBreakIterator,
                          WeakCallbackType::kInternalFields);
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

  int length = text->length();
  text = String::Flatten(text);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = text->GetFlatContent();
  base::SmartArrayPointer<uc16> sap;
  const UChar* text_value = GetUCharBufferFromFlat(flat, &sap, length);
  u_text = new icu::UnicodeString(text_value, length);
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

namespace {
void ConvertCaseWithTransliterator(icu::UnicodeString* input,
                                   const char* transliterator_id) {
  UErrorCode status = U_ZERO_ERROR;
  base::SmartPointer<icu::Transliterator> translit(
      icu::Transliterator::createInstance(
          icu::UnicodeString(transliterator_id, -1, US_INV), UTRANS_FORWARD,
          status));
  if (U_FAILURE(status)) return;
  translit->transliterate(*input);
}

MUST_USE_RESULT Object* LocaleConvertCase(Handle<String> s, Isolate* isolate,
                                          bool is_to_upper, const char* lang) {
  int32_t src_length = s->length();

  // Greek uppercasing has to be done via transliteration.
  // TODO(jshin): Drop this special-casing once ICU's regular case conversion
  // API supports Greek uppercasing. See
  // http://bugs.icu-project.org/trac/ticket/10582 .
  // In the meantime, if there's no Greek character in |s|, call this
  // function again with the root locale (lang="").
  // ICU's C API for transliteration is nasty and we just use C++ API.
  if (V8_UNLIKELY(is_to_upper && lang[0] == 'e' && lang[1] == 'l')) {
    icu::UnicodeString converted;
    base::SmartArrayPointer<uc16> sap;
    {
      DisallowHeapAllocation no_gc;
      String::FlatContent flat = s->GetFlatContent();
      const UChar* src = GetUCharBufferFromFlat(flat, &sap, src_length);
      // Starts with the source string (read-only alias with copy-on-write
      // semantics) and will be modified to contain the converted result.
      // Using read-only alias at first saves one copy operation if
      // transliteration does not change the input, which is rather rare.
      // Moreover, transliteration takes rather long so that saving one copy
      // helps only a little bit.
      converted.setTo(false, src, src_length);
      ConvertCaseWithTransliterator(&converted, "el-Upper");
      // If no change is made, just return |s|.
      if (converted.getBuffer() == src) return *s;
    }
    Handle<String> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
            reinterpret_cast<const uint16_t*>(converted.getBuffer()),
            converted.length())));
    return *result;
  }

  auto case_converter = is_to_upper ? u_strToUpper : u_strToLower;

  int32_t dest_length = src_length;
  UErrorCode status;
  Handle<SeqTwoByteString> result;
  base::SmartArrayPointer<uc16> sap;

  // This is not a real loop. It'll be executed only once (no overflow) or
  // twice (overflow).
  for (int i = 0; i < 2; ++i) {
    result =
        isolate->factory()->NewRawTwoByteString(dest_length).ToHandleChecked();
    DisallowHeapAllocation no_gc;
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
         ~(((ch >= 'a' && ch <= 'z') || (((ch & 0xE0) == 0xE0) && ch != 0xE7))
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
bool ToUpperOneByte(const Vector<const Char>& src,
                    Handle<SeqOneByteString> result, int* sharp_s_count) {
  // Still pretty-fast path for the input with non-ASCII Latin-1 characters.

  // There are two special cases.
  //  1. U+00B5 and U+00FF are mapped to a character beyond U+00FF.
  //  2. Lower case sharp-S converts to "SS" (two characters)
  *sharp_s_count = 0;
  int32_t index = 0;
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
    result->SeqOneByteStringSet(index++, ToLatin1Upper(ch));
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

}  // namespace

RUNTIME_FUNCTION(Runtime_StringToLowerCaseI18N) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);

  int length = s->length();
  s = String::Flatten(s);
  // First scan the string for uppercase and non-ASCII characters:
  if (s->HasOnlyOneByteChars()) {
    unsigned first_index_to_lower = length;
    for (int index = 0; index < length; ++index) {
      // Blink specializes this path for one-byte strings, so it
      // does not need to do a generic get, but can do the equivalent
      // of SeqOneByteStringGet.
      uint16_t ch = s->Get(index);
      if (V8_UNLIKELY(IsASCIIUpper(ch) || ch & ~0x7F)) {
        first_index_to_lower = index;
        break;
      }
    }

    // Nothing to do if the string is all ASCII with no uppercase.
    if (first_index_to_lower == length) return *s;

    // We depend here on the invariant that the length of a Latin1
    // string is invariant under ToLowerCase, and the result always
    // fits in the Latin1 range in the *root locale*. It does not hold
    // for ToUpperCase even in the root locale.
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawOneByteString(length));

    DisallowHeapAllocation no_gc;
    String::FlatContent flat = s->GetFlatContent();
    if (flat.IsOneByte()) {
      const uint8_t* src = flat.ToOneByteVector().start();
      CopyChars(result->GetChars(), src, first_index_to_lower);
      for (int index = first_index_to_lower; index < length; ++index) {
        uint16_t ch = static_cast<uint16_t>(src[index]);
        result->SeqOneByteStringSet(index, ToLatin1Lower(ch));
      }
    } else {
      const uint16_t* src = flat.ToUC16Vector().start();
      CopyChars(result->GetChars(), src, first_index_to_lower);
      for (int index = first_index_to_lower; index < length; ++index) {
        uint16_t ch = src[index];
        result->SeqOneByteStringSet(index, ToLatin1Lower(ch));
      }
    }

    return *result;
  }

  // Blink had an additional case here for ASCII 2-byte strings, but
  // that is subsumed by the above code (assuming there isn't a false
  // negative for HasOnlyOneByteChars).

  // Do a slower implementation for cases that include non-ASCII characters.
  return LocaleConvertCase(s, isolate, false, "");
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseI18N) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);

  // This function could be optimized for no-op cases the way lowercase
  // counterpart is, but in empirical testing, few actual calls to upper()
  // are no-ops. So, it wouldn't be worth the extra time for pre-scanning.

  int32_t length = s->length();
  s = String::Flatten(s);

  if (s->HasOnlyOneByteChars()) {
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawOneByteString(length));

    int sharp_s_count;
    bool is_result_single_byte;
    {
      DisallowHeapAllocation no_gc;
      String::FlatContent flat = s->GetFlatContent();
      // If it was ok to slow down ASCII-only input slightly, ToUpperFastASCII
      // could be removed  because ToUpperOneByte is pretty fast now (it
      // does not call ICU API any more.).
      if (flat.IsOneByte()) {
        Vector<const uint8_t> src = flat.ToOneByteVector();
        if (ToUpperFastASCII(src, result)) return *result;
        is_result_single_byte = ToUpperOneByte(src, result, &sharp_s_count);
      } else {
        DCHECK(flat.IsTwoByte());
        Vector<const uint16_t> src = flat.ToUC16Vector();
        if (ToUpperFastASCII(src, result)) return *result;
        is_result_single_byte = ToUpperOneByte(src, result, &sharp_s_count);
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

RUNTIME_FUNCTION(Runtime_StringLocaleConvertCase) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 3);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_upper, 1);
  CONVERT_ARG_HANDLE_CHECKED(SeqOneByteString, lang, 2);

  // All the languages requiring special handling ("az", "el", "lt", "tr")
  // have a 2-letter language code.
  DCHECK(lang->length() == 2);
  uint8_t lang_str[3];
  memcpy(lang_str, lang->GetChars(), 2);
  lang_str[2] = 0;
  s = String::Flatten(s);
  // TODO(jshin): Consider adding a fast path for ASCII or Latin-1. The fastpath
  // in the root locale needs to be adjusted for az, lt and tr because even case
  // mapping of ASCII range characters are different in those locales.
  // Greek (el) does not require any adjustment, though.
  return LocaleConvertCase(s, isolate, is_upper,
                           reinterpret_cast<const char*>(lang_str));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_I18N_SUPPORT
