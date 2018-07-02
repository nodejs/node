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

  // Layout description.
  static const int kDecimalFormat = JSObject::kHeaderSize;
  static const int kSize = kDecimalFormat + kPointerSize;

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
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_INTL_OBJECTS_H_
