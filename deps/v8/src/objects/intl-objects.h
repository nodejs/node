// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_INTL_OBJECTS_H_
#define V8_OBJECTS_INTL_OBJECTS_H_

#include <set>
#include <string>

#include "src/contexts.h"
#include "src/intl.h"
#include "src/objects.h"
#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
class Collator;
class DecimalFormat;
class PluralRules;
class SimpleDateFormat;
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
  static icu::SimpleDateFormat* UnpackDateFormat(Isolate* isolate,
                                                 Handle<JSObject> obj);

  // Release memory we allocated for the DateFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteDateFormat(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kSimpleDateFormat = JSObject::kHeaderSize;
  static const int kSize = kSimpleDateFormat + kPointerSize;

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
  static icu::DecimalFormat* UnpackNumberFormat(Isolate* isolate,
                                                Handle<JSObject> obj);

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
  static MaybeHandle<Object> FormatNumber(Isolate* isolate,
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

class Collator {
 public:
  // Create a collator for the specificied locale and options. Stores the
  // collator in the provided collator_holder.
  static bool InitializeCollator(Isolate* isolate,
                                 Handle<JSObject> collator_holder,
                                 Handle<String> locale,
                                 Handle<JSObject> options,
                                 Handle<JSObject> resolved);

  // Unpacks collator object from corresponding JavaScript object.
  static icu::Collator* UnpackCollator(Isolate* isolate, Handle<JSObject> obj);

  // Layout description.
  static const int kCollator = JSObject::kHeaderSize;
  static const int kSize = kCollator + kPointerSize;

 private:
  Collator();
};

class PluralRules {
 public:
  // Create a PluralRules and DecimalFormat for the specificied locale and
  // options. Returns false on an ICU failure.
  static bool InitializePluralRules(Isolate* isolate, Handle<String> locale,
                                    Handle<JSObject> options,
                                    Handle<JSObject> resolved,
                                    icu::PluralRules** plural_rules,
                                    icu::DecimalFormat** decimal_format);

  // Unpacks PluralRules object from corresponding JavaScript object.
  static icu::PluralRules* UnpackPluralRules(Isolate* isolate,
                                             Handle<JSObject> obj);

  // Unpacks NumberFormat object from corresponding JavaScript PluralRUles
  // object.
  static icu::DecimalFormat* UnpackNumberFormat(Isolate* isolate,
                                                Handle<JSObject> obj);

  // Release memory we allocated for the Collator once the JS object that holds
  // the pointer gets garbage collected.
  static void DeletePluralRules(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kPluralRules = JSObject::kHeaderSize;
  // Values are formatted with this NumberFormat and then parsed as a Number
  // to round them based on the options passed into the PluralRules objct.
  // TODO(littledan): If a future version of ICU supports the rounding
  // built-in to PluralRules, switch to that, see this bug:
  // http://bugs.icu-project.org/trac/ticket/12763
  static const int kNumberFormat = kPluralRules + kPointerSize;
  static const int kSize = kNumberFormat + kPointerSize;

 private:
  PluralRules();
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
  static icu::BreakIterator* UnpackBreakIterator(Isolate* isolate,
                                                 Handle<JSObject> obj);

  // Release memory we allocated for the BreakIterator once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteBreakIterator(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kBreakIterator = JSObject::kHeaderSize;
  static const int kUnicodeString = kBreakIterator + kPointerSize;
  static const int kSize = kUnicodeString + kPointerSize;

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

  // Gets the ICU locales for a given service. If there is a locale with a
  // script tag then the locales also include a locale without the script; eg,
  // pa_Guru_IN (language=Panjabi, script=Gurmukhi, country-India) would include
  // pa_IN.
  static std::set<std::string> GetAvailableLocales(const IcuService& service);

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
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_INTL_OBJECTS_H_
