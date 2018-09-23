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
class BreakIterator;
class Collator;
class DecimalFormat;
class PluralRules;
class SimpleDateFormat;
class UnicodeString;
}

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class DateFormat {
 public:
  // Create a formatter for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::SimpleDateFormat* InitializeDateTimeFormat(
      Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks date format object from corresponding JavaScript object.
  static icu::SimpleDateFormat* UnpackDateFormat(Handle<JSObject> obj);

  // Release memory we allocated for the DateFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteDateFormat(const v8::WeakCallbackInfo<void>& data);

  // ecma402/#sec-formatdatetime
  // FormatDateTime( dateTimeFormat, x )
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> FormatDateTime(
      Isolate* isolate, Handle<JSObject> date_time_format_holder, double x);

  // ecma402/#sec-datetime-format-functions
  // DateTime Format Functions
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> DateTimeFormat(
      Isolate* isolate, Handle<JSObject> date_time_format_holder,
      Handle<Object> date);

  // The UnwrapDateTimeFormat abstract operation gets the underlying
  // DateTimeFormat operation for various methods which implement ECMA-402 v1
  // semantics for supporting initializing existing Intl objects.
  //
  // ecma402/#sec-unwrapdatetimeformat
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> Unwrap(
      Isolate* isolate, Handle<JSReceiver> receiver, const char* method_name);

  // ecma-402/#sec-todatetimeoptions
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> ToDateTimeOptions(
      Isolate* isolate, Handle<Object> input_options, const char* required,
      const char* defaults);

  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToLocaleDateTime(
      Isolate* isolate, Handle<Object> date, Handle<Object> locales,
      Handle<Object> options, const char* required, const char* defaults,
      const char* service);

  // Layout description.
#define DATE_FORMAT_FIELDS(V)        \
  V(kSimpleDateFormat, kPointerSize) \
  V(kBoundFormat, kPointerSize)      \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, DATE_FORMAT_FIELDS)
#undef DATE_FORMAT_FIELDS

  // ContextSlot defines the context structure for the bound
  // DateTimeFormat.prototype.format function
  enum ContextSlot {
    kDateFormat = Context::MIN_CONTEXT_SLOTS,

    kLength
  };

  // TODO(ryzokuken): Remove this and use regular accessors once DateFormat is a
  // subclass of JSObject
  //
  // This needs to be consistent with the above Layout Description
  static const int kSimpleDateFormatIndex = 0;
  static const int kBoundFormatIndex = 1;

 private:
  DateFormat();
};

class NumberFormat {
 public:
  // Create a formatter for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::DecimalFormat* InitializeNumberFormat(Isolate* isolate,
                                                    Handle<String> locale,
                                                    Handle<JSObject> options,
                                                    Handle<JSObject> resolved);

  // Unpacks number format object from corresponding JavaScript object.
  static icu::DecimalFormat* UnpackNumberFormat(Handle<JSObject> obj);

  // Release memory we allocated for the NumberFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteNumberFormat(const v8::WeakCallbackInfo<void>& data);

  // The UnwrapNumberFormat abstract operation gets the underlying
  // NumberFormat operation for various methods which implement
  // ECMA-402 v1 semantics for supporting initializing existing Intl
  // objects.
  //
  // ecma402/#sec-unwrapnumberformat
  static MaybeHandle<JSObject> Unwrap(Isolate* isolate,
                                      Handle<JSReceiver> receiver,
                                      const char* method_name);

  // ecm402/#sec-formatnumber
  static MaybeHandle<String> FormatNumber(Isolate* isolate,
                                          Handle<JSObject> number_format_holder,
                                          double value);

  // Layout description.
#define NUMBER_FORMAT_FIELDS(V)   \
  /* Pointer fields. */           \
  V(kDecimalFormat, kPointerSize) \
  V(kBoundFormat, kPointerSize)   \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, NUMBER_FORMAT_FIELDS)
#undef NUMBER_FORMAT_FIELDS

  // ContextSlot defines the context structure for the bound
  // NumberFormat.prototype.format function.
  enum ContextSlot {
    // The number format instance that the function holding this
    // context is bound to.
    kNumberFormat = Context::MIN_CONTEXT_SLOTS,

    kLength
  };

  // TODO(gsathya): Remove this and use regular accessors once
  // NumberFormat is a sub class of JSObject.
  //
  // This needs to be consistent with the above LayoutDescription.
  static const int kDecimalFormatIndex = 0;
  static const int kBoundFormatIndex = 1;

 private:
  NumberFormat();
};

class V8BreakIterator {
 public:
  // Create a BreakIterator for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::BreakIterator* InitializeBreakIterator(Isolate* isolate,
                                                     Handle<String> locale,
                                                     Handle<JSObject> options,
                                                     Handle<JSObject> resolved);

  // Unpacks break iterator object from corresponding JavaScript object.
  static icu::BreakIterator* UnpackBreakIterator(Handle<JSObject> obj);

  // Release memory we allocated for the BreakIterator once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteBreakIterator(const v8::WeakCallbackInfo<void>& data);

  static void AdoptText(Isolate* isolate,
                        Handle<JSObject> break_iterator_holder,
                        Handle<String> text);

  // Layout description.
#define BREAK_ITERATOR_FIELDS(V)   \
  /* Pointer fields. */            \
  V(kBreakIterator, kPointerSize)  \
  V(kUnicodeString, kPointerSize)  \
  V(kBoundAdoptText, kPointerSize) \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, BREAK_ITERATOR_FIELDS)
#undef BREAK_ITERATOR_FIELDS

  // ContextSlot defines the context structure for the bound
  // v8BreakIterator.prototype.adoptText function
  enum class ContextSlot {
    kV8BreakIterator = Context::MIN_CONTEXT_SLOTS,

    kLength
  };

  // TODO(ryzokuken): Remove this and use regular accessors once v8BreakIterator
  // is a subclass of JSObject
  //
  // This needs to be consistent with the above Layour Description
  static const int kBreakIteratorIndex = 0;
  static const int kUnicodeStringIndex = 1;
  static const int kBoundAdoptTextIndex = 2;

 private:
  V8BreakIterator();
};

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

  inline static Intl::Type TypeFromInt(int type);
  inline static Intl::Type TypeFromSmi(Smi* type);

  // Checks if the given object has the expected_type based by looking
  // up a private symbol on the object.
  //
  // TODO(gsathya): This should just be an instance type check once we
  // move all the Intl objects to C++.
  static bool IsObjectOfType(Isolate* isolate, Handle<Object> object,
                             Intl::Type expected_type);

  static IcuService StringToIcuService(Handle<String> service);

  // Gets the ICU locales for a given service. If there is a locale with a
  // script tag then the locales also include a locale without the script; eg,
  // pa_Guru_IN (language=Panjabi, script=Gurmukhi, country-India) would include
  // pa_IN.
  static std::set<std::string> GetAvailableLocales(const IcuService& service);

  static V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> AvailableLocalesOf(
      Isolate* isolate, Handle<String> service);

  static MaybeHandle<JSObject> SupportedLocalesOf(Isolate* isolate,
                                                  Handle<String> service,
                                                  Handle<Object> locales_in,
                                                  Handle<Object> options_in);

  static std::string DefaultLocale(Isolate* isolate);

  static void DefineWEProperty(Isolate* isolate, Handle<JSObject> target,
                               Handle<Name> key, Handle<Object> value);

  // If locale has a script tag then return true and the locale without the
  // script else return false and an empty string
  static bool RemoveLocaleScriptTag(const std::string& icu_locale,
                                    std::string* locale_less_script);

  // Returns the underlying Intl receiver for various methods which
  // implement ECMA-402 v1 semantics for supporting initializing
  // existing Intl objects.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> UnwrapReceiver(
      Isolate* isolate, Handle<JSReceiver> receiver,
      Handle<JSFunction> constructor, Intl::Type type,
      Handle<String> method_name /* TODO(gsathya): Make this char const* */,
      bool check_legacy_constructor = false);

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

  // ecma-402/#sec-currencydigits
  // The currency is expected to an all upper case string value.
  static Handle<Smi> CurrencyDigits(Isolate* isolate, Handle<String> currency);

  // TODO(ftang): Remove this and use ICU to the conversion in the future
  static void ParseExtension(Isolate* isolate, const std::string& extension,
                             std::map<std::string, std::string>& out);

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> CreateNumberFormat(
      Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
      Handle<JSObject> resolved);

  // ecma402/#sec-iswellformedcurrencycode
  static bool IsWellFormedCurrencyCode(Isolate* isolate,
                                       Handle<String> currency);

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

  // ecma402/#sec-defaultnumberoption
  V8_WARN_UNUSED_RESULT static Maybe<int> DefaultNumberOption(
      Isolate* isolate, Handle<Object> value, int min, int max, int fallback,
      Handle<String> property);

  // ecma402/#sec-getnumberoption
  V8_WARN_UNUSED_RESULT static Maybe<int> GetNumberOption(
      Isolate* isolate, Handle<JSReceiver> options, Handle<String> property,
      int min, int max, int fallback);
  V8_WARN_UNUSED_RESULT static Maybe<int> GetNumberOption(
      Isolate* isolate, Handle<JSReceiver> options, const char* property,
      int min, int max, int fallback);

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
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_INTL_OBJECTS_H_
