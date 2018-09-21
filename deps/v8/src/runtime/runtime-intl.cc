// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <cmath>
#include <memory>

#include "src/api-inl.h"
#include "src/api-natives.h"
#include "src/arguments-inl.h"
#include "src/date.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/intl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/objects/intl-objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-list-format.h"
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/managed.h"
#include "src/runtime/runtime-utils.h"
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
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"


namespace v8 {
namespace internal {

// ecma402 #sec-formatlist
RUNTIME_FUNCTION(Runtime_FormatList) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSListFormat, list_format, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, list, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSListFormat::FormatList(isolate, list_format, list));
}

// ecma402 #sec-formatlisttoparts
RUNTIME_FUNCTION(Runtime_FormatListToParts) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSListFormat, list_format, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, list, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSListFormat::FormatListToParts(isolate, list_format, list));
}

RUNTIME_FUNCTION(Runtime_GetNumberOption) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, options, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, property, 1);
  CONVERT_SMI_ARG_CHECKED(min, 2);
  CONVERT_SMI_ARG_CHECKED(max, 3);
  CONVERT_SMI_ARG_CHECKED(fallback, 4);

  Maybe<int> num =
      Intl::GetNumberOption(isolate, options, property, min, max, fallback);
  if (num.IsNothing()) {
    return ReadOnlyRoots(isolate).exception();
  }
  return Smi::FromInt(num.FromJust());
}

RUNTIME_FUNCTION(Runtime_DefaultNumberOption) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_SMI_ARG_CHECKED(min, 1);
  CONVERT_SMI_ARG_CHECKED(max, 2);
  CONVERT_SMI_ARG_CHECKED(fallback, 3);
  CONVERT_ARG_HANDLE_CHECKED(String, property, 4);

  Maybe<int> num =
      Intl::DefaultNumberOption(isolate, value, min, max, fallback, property);
  if (num.IsNothing()) {
    return ReadOnlyRoots(isolate).exception();
  }
  return Smi::FromInt(num.FromJust());
}

// ECMA 402 6.2.3
RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, locale, 0);

  std::string canonicalized;
  if (!Intl::CanonicalizeLanguageTag(isolate, locale).To(&canonicalized)) {
    return ReadOnlyRoots(isolate).exception();
  }
  return *isolate->factory()->NewStringFromAsciiChecked(canonicalized.c_str());
}

RUNTIME_FUNCTION(Runtime_AvailableLocalesOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);
  Handle<JSObject> locales;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, locales, Intl::AvailableLocalesOf(isolate, service));
  return *locales;
}

RUNTIME_FUNCTION(Runtime_GetDefaultICULocale) {
  HandleScope scope(isolate);

  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewStringFromAsciiChecked(
      Intl::DefaultLocale(isolate).c_str());
}

RUNTIME_FUNCTION(Runtime_IsWellFormedCurrencyCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, currency, 0);
  return *(isolate->factory()->ToBoolean(
      Intl::IsWellFormedCurrencyCode(isolate, currency)));
}

RUNTIME_FUNCTION(Runtime_DefineWEProperty) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Intl::DefineWEProperty(isolate, target, key, value);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  CONVERT_SMI_ARG_CHECKED(expected_type_int, 1);

  Intl::Type expected_type = Intl::TypeFromInt(expected_type_int);

  return isolate->heap()->ToBoolean(
      Intl::IsObjectOfType(isolate, input, expected_type));
}

RUNTIME_FUNCTION(Runtime_MarkAsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, type, 1);

#ifdef DEBUG
  // TypeFromSmi does correctness checks.
  Intl::Type type_intl = Intl::TypeFromSmi(*type);
  USE(type_intl);
#endif

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  JSObject::SetProperty(isolate, input, marker, type, LanguageMode::kStrict)
      .Assert();

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_CreateDateTimeFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_date_time_format_function(), isolate);

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set date time formatter as embedder field of the resulting JS object.
  icu::SimpleDateFormat* date_format =
      DateFormat::InitializeDateTimeFormat(isolate, locale, options, resolved);
  CHECK_NOT_NULL(date_format);

  local_object->SetEmbedderField(DateFormat::kSimpleDateFormatIndex,
                                 reinterpret_cast<Smi*>(date_format));

  // Make object handle weak so we can delete the data format once GC kicks in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          DateFormat::DeleteDateFormat,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_CreateNumberFormat) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);
  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::CreateNumberFormat(isolate, locale, options, resolved));
}

RUNTIME_FUNCTION(Runtime_CurrencyDigits) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, currency, 0);
  return *Intl::CurrencyDigits(isolate, currency);
}

RUNTIME_FUNCTION(Runtime_CollatorResolvedOptions) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, collator_obj, 0);

  // 3. If pr does not have an [[InitializedCollator]] internal
  // slot, throw a TypeError exception.
  if (!collator_obj->IsJSCollator()) {
    Handle<String> method_str = isolate->factory()->NewStringFromStaticChars(
        "Intl.Collator.prototype.resolvedOptions");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              method_str, collator_obj));
  }

  Handle<JSCollator> collator = Handle<JSCollator>::cast(collator_obj);

  return *JSCollator::ResolvedOptions(isolate, collator);
}

RUNTIME_FUNCTION(Runtime_PluralRulesResolvedOptions) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, plural_rules_obj, 0);

  // 3. If pr does not have an [[InitializedPluralRules]] internal
  // slot, throw a TypeError exception.
  if (!plural_rules_obj->IsJSPluralRules()) {
    Handle<String> method_str = isolate->factory()->NewStringFromStaticChars(
        "Intl.PluralRules.prototype.resolvedOptions");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              method_str, plural_rules_obj));
  }

  Handle<JSPluralRules> plural_rules =
      Handle<JSPluralRules>::cast(plural_rules_obj);

  return *JSPluralRules::ResolvedOptions(isolate, plural_rules);
}

RUNTIME_FUNCTION(Runtime_ParseExtension) {
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, extension, 0);
  std::map<std::string, std::string> map;
  Intl::ParseExtension(isolate, std::string(extension->ToCString().get()), map);
  Handle<JSObject> extension_map =
      isolate->factory()->NewJSObjectWithNullProto();
  for (std::map<std::string, std::string>::iterator it = map.begin();
       it != map.end(); it++) {
    JSObject::AddProperty(
        isolate, extension_map,
        factory->NewStringFromAsciiChecked(it->first.c_str()),
        factory->NewStringFromAsciiChecked(it->second.c_str()), NONE);
  }
  return *extension_map;
}

RUNTIME_FUNCTION(Runtime_PluralRulesSelect) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, plural_rules_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  // 3. If pr does not have an [[InitializedPluralRules]] internal
  // slot, throw a TypeError exception.
  if (!plural_rules_obj->IsJSPluralRules()) {
    Handle<String> method_str = isolate->factory()->NewStringFromStaticChars(
        "Intl.PluralRules.prototype.select");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              method_str, plural_rules_obj));
  }

  Handle<JSPluralRules> plural_rules =
      Handle<JSPluralRules>::cast(plural_rules_obj);

  // 4. Return ? ResolvePlural(pr, n).

  RETURN_RESULT_OR_FAILURE(
      isolate, JSPluralRules::ResolvePlural(isolate, plural_rules, number));
}

RUNTIME_FUNCTION(Runtime_CreateBreakIterator) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<JSFunction> constructor(
      isolate->native_context()->intl_v8_break_iterator_function(), isolate);

  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, local_object,
                                     JSObject::New(constructor, constructor));

  // Set break iterator as embedder field of the resulting JS object.
  icu::BreakIterator* break_iterator = V8BreakIterator::InitializeBreakIterator(
      isolate, locale, options, resolved);
  CHECK_NOT_NULL(break_iterator);

  if (!break_iterator) return isolate->ThrowIllegalOperation();

  local_object->SetEmbedderField(V8BreakIterator::kBreakIteratorIndex,
                                 reinterpret_cast<Smi*>(break_iterator));
  // Make sure that the pointer to adopted text is nullptr.
  local_object->SetEmbedderField(V8BreakIterator::kUnicodeStringIndex,
                                 static_cast<Smi*>(nullptr));

  // Make object handle weak so we can delete the break iterator once GC kicks
  // in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          V8BreakIterator::DeleteBreakIterator,
                          WeakCallbackType::kInternalFields);
  return *local_object;
}

RUNTIME_FUNCTION(Runtime_BreakIteratorFirst) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->first());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorNext) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->next());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorCurrent) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  return *isolate->factory()->NewNumberFromInt(break_iterator->current());
}

RUNTIME_FUNCTION(Runtime_BreakIteratorBreakType) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return *isolate->factory()->NewStringFromStaticChars("none");
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return ReadOnlyRoots(isolate).number_string();
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

RUNTIME_FUNCTION(Runtime_ToLocaleDateTime) {
  HandleScope scope(isolate);

  DCHECK_EQ(6, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, date, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, locales, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, options, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, required, 3);
  CONVERT_ARG_HANDLE_CHECKED(String, defaults, 4);
  CONVERT_ARG_HANDLE_CHECKED(String, service, 5);

  RETURN_RESULT_OR_FAILURE(
      isolate, DateFormat::ToLocaleDateTime(
                   isolate, date, locales, options, required->ToCString().get(),
                   defaults->ToCString().get(), service->ToCString().get()));
}

RUNTIME_FUNCTION(Runtime_ToDateTimeOptions) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 3);
  CONVERT_ARG_HANDLE_CHECKED(Object, options, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, required, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, defaults, 2);
  RETURN_RESULT_OR_FAILURE(
      isolate, DateFormat::ToDateTimeOptions(isolate, options,
                                             required->ToCString().get(),
                                             defaults->ToCString().get()));
}

RUNTIME_FUNCTION(Runtime_StringToLowerCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  RETURN_RESULT_OR_FAILURE(isolate, ConvertToLower(s, isolate));
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  RETURN_RESULT_OR_FAILURE(isolate, ConvertToUpper(s, isolate));
}

RUNTIME_FUNCTION(Runtime_DateCacheVersion) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  if (isolate->serializer_enabled())
    return ReadOnlyRoots(isolate).undefined_value();
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

RUNTIME_FUNCTION(Runtime_IntlUnwrapReceiver) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(type_int, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, method, 3);
  CONVERT_BOOLEAN_ARG_CHECKED(check_legacy_constructor, 4);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::UnwrapReceiver(isolate, receiver, constructor,
                                    Intl::TypeFromInt(type_int), method,
                                    check_legacy_constructor));
}

RUNTIME_FUNCTION(Runtime_SupportedLocalesOf) {
  HandleScope scope(isolate);

  DCHECK_EQ(args.length(), 3);

  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, locales, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, options, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(isolate, service, locales, options));
}

}  // namespace internal
}  // namespace v8
