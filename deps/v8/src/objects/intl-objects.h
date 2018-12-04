// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_INTL_OBJECTS_H_
#define V8_OBJECTS_INTL_OBJECTS_H_

#include <map>
#include <set>
#include <string>

#include "src/contexts.h"
#include "src/intl.h"
#include "src/objects.h"
#include "unicode/locid.h"
#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class DecimalFormat;
class SimpleDateFormat;
class UnicodeString;
}

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class JSCollator;

class Intl {
 public:
  enum Type {
    kNumberFormat = 0,
    kCollator,
    kDateTimeFormat,
    kPluralRules,
    kBreakIterator,
    kLocale,

    kTypeCount
  };

  enum class BoundFunctionContextSlot {
    kBoundFunction = Context::MIN_CONTEXT_SLOTS,
    kLength
  };

  inline static Intl::Type TypeFromInt(int type);
  inline static Intl::Type TypeFromSmi(Smi* type);

  // Checks if the given object has the expected_type based by looking
  // up a private symbol on the object.
  //
  // TODO(gsathya): This should just be an instance type check once we
  // move all the Intl objects to C++.
  static bool IsObjectOfType(Isolate* isolate, Handle<Object> object,
                             Intl::Type expected_type);

  // Gets the ICU locales for a given service. If there is a locale with a
  // script tag then the locales also include a locale without the script; eg,
  // pa_Guru_IN (language=Panjabi, script=Gurmukhi, country-India) would include
  // pa_IN.
  static std::set<std::string> GetAvailableLocales(ICUService service);

  // Get the name of the numbering system from locale.
  // ICU doesn't expose numbering system in any way, so we have to assume that
  // for given locale NumberingSystem constructor produces the same digits as
  // NumberFormat/Calendar would.
  static std::string GetNumberingSystem(const icu::Locale& icu_locale);

  static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> AvailableLocalesOf(
      Isolate* isolate, Handle<String> service);

  static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> SupportedLocalesOf(
      Isolate* isolate, ICUService service, Handle<Object> locales_in,
      Handle<Object> options_in);

  static std::string DefaultLocale(Isolate* isolate);

  // The ResolveLocale abstract operation compares a BCP 47 language
  // priority list requestedLocales against the locales in
  // availableLocales and determines the best available language to
  // meet the request. availableLocales, requestedLocales, and
  // relevantExtensionKeys must be provided as List values, options
  // and localeData as Records.
  //
  // #ecma402/sec-partitiondatetimepattern
  //
  // Returns a JSObject with two properties:
  //   (1) locale
  //   (2) extension
  //
  // To access either, use JSObject::GetDataProperty.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> ResolveLocale(
      Isolate* isolate, const char* service, Handle<Object> requestedLocales,
      Handle<Object> options);

  // This currently calls out to the JavaScript implementation of
  // CanonicalizeLocaleList.
  // Note: This is deprecated glue code, required only as long as ResolveLocale
  // still calls a JS implementation. The C++ successor is the overloaded
  // version below that returns a Maybe<std::vector<std::string>>.
  //
  // ecma402/#sec-canonicalizelocalelist
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> CanonicalizeLocaleListJS(
      Isolate* isolate, Handle<Object> locales);

  // ECMA402 9.2.10. GetOption( options, property, type, values, fallback)
  // ecma402/#sec-getoption
  //
  // This is specialized for the case when type is string.
  //
  // Instead of passing undefined for the values argument as the spec
  // defines, pass in an empty vector.
  //
  // Returns true if options object has the property and stores the
  // result in value. Returns false if the value is not found. The
  // caller is required to use fallback value appropriately in this
  // case.
  //
  // service is a string denoting the type of Intl object; used when
  // printing the error message.
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetStringOption(
      Isolate* isolate, Handle<JSReceiver> options, const char* property,
      std::vector<const char*> values, const char* service,
      std::unique_ptr<char[]>* result);

  // ECMA402 9.2.10. GetOption( options, property, type, values, fallback)
  // ecma402/#sec-getoption
  //
  // This is specialized for the case when type is boolean.
  //
  // Returns true if options object has the property and stores the
  // result in value. Returns false if the value is not found. The
  // caller is required to use fallback value appropriately in this
  // case.
  //
  // service is a string denoting the type of Intl object; used when
  // printing the error message.
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetBoolOption(
      Isolate* isolate, Handle<JSReceiver> options, const char* property,
      const char* service, bool* result);

  // Canonicalize the locale.
  // https://tc39.github.io/ecma402/#sec-canonicalizelanguagetag,
  // including type check and structural validity check.
  static Maybe<std::string> CanonicalizeLanguageTag(Isolate* isolate,
                                                    Handle<Object> locale_in);

  // https://tc39.github.io/ecma402/#sec-canonicalizelocalelist
  // {only_return_one_result} is an optimization for callers that only
  // care about the first result.
  static Maybe<std::vector<std::string>> CanonicalizeLocaleList(
      Isolate* isolate, Handle<Object> locales,
      bool only_return_one_result = false);

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> CreateNumberFormat(
      Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
      Handle<JSObject> resolved);

  // For locale sensitive functions
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> StringLocaleConvertCase(
      Isolate* isolate, Handle<String> s, bool is_upper,
      Handle<Object> locales);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> StringLocaleCompare(
      Isolate* isolate, Handle<String> s1, Handle<String> s2,
      Handle<Object> locales, Handle<Object> options);

  V8_WARN_UNUSED_RESULT static Handle<Object> CompareStrings(
      Isolate* isolate, Handle<JSCollator> collator, Handle<String> s1,
      Handle<String> s2);

  // ecma402/#sup-properties-of-the-number-prototype-object
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> NumberToLocaleString(
      Isolate* isolate, Handle<Object> num, Handle<Object> locales,
      Handle<Object> options);

  // ecma402/#sec-setnfdigitoptions
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetNumberFormatDigitOptions(
      Isolate* isolate, icu::DecimalFormat* number_format,
      Handle<JSReceiver> options, int mnfd_default, int mxfd_default);

  icu::Locale static CreateICULocale(Isolate* isolate,
                                     Handle<String> bcp47_locale_str);

  // Helper funciton to convert a UnicodeString to a Handle<String>
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToString(
      Isolate* isolate, const icu::UnicodeString& string);

  // Helper function to convert a substring of UnicodeString to a Handle<String>
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToString(
      Isolate* isolate, const icu::UnicodeString& string, int32_t begin,
      int32_t end);

  // A helper function to implement formatToParts which add element to array as
  // $array[$index] = { type: $field_type_string, value: $value }
  static void AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                         Handle<String> field_type_string,
                         Handle<String> value);

  // A helper function to implement formatToParts which add element to array as
  // $array[$index] = {
  //   type: $field_type_string, value: $value,
  //   $additional_property_name: $additional_property_value
  // }
  static void AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                         Handle<String> field_type_string, Handle<String> value,
                         Handle<String> additional_property_name,
                         Handle<String> additional_property_value);

  // A helper function to help handle Unicode Extensions in locale.
  static std::map<std::string, std::string> LookupUnicodeExtensions(
    const icu::Locale& icu_locale, const std::set<std::string>& relevant_keys);

  // In ECMA 402 v1, Intl constructors supported a mode of operation
  // where calling them with an existing object as a receiver would
  // transform the receiver into the relevant Intl instance with all
  // internal slots. In ECMA 402 v2, this capability was removed, to
  // avoid adding internal slots on existing objects. In ECMA 402 v3,
  // the capability was re-added as "normative optional" in a mode
  // which chains the underlying Intl instance on any object, when the
  // constructor is called
  //
  // See ecma402/#legacy-constructor.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> LegacyUnwrapReceiver(
      Isolate* isolate, Handle<JSReceiver> receiver,
      Handle<JSFunction> constructor, bool has_initialized_slot);

  // A factory method to got cached objects.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> CachedOrNewService(
      Isolate* isolate, Handle<String> service, Handle<Object> locales,
      Handle<Object> options, Handle<Object> internal_options);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_INTL_OBJECTS_H_
