// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_LOCALE_H_
#define V8_OBJECTS_JS_LOCALE_H_

#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "unicode/unistr.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class JSLocale : public JSObject {
 public:
  // Initializes locale object with properties derived from input locale string
  // and options.
  static MaybeHandle<JSLocale> Initialize(Isolate* isolate,
                                          Handle<JSLocale> locale_holder,
                                          Handle<String> locale,
                                          Handle<JSReceiver> options);
  static Handle<String> Maximize(Isolate* isolate, String* locale);
  static Handle<String> Minimize(Isolate* isolate, String* locale);

  Handle<String> CaseFirstAsString() const;
  Handle<String> NumericAsString() const;
  Handle<String> HourCycleAsString() const;

  DECL_CAST(JSLocale)

  // Locale accessors.
  DECL_ACCESSORS(language, Object)
  DECL_ACCESSORS(script, Object)
  DECL_ACCESSORS(region, Object)
  DECL_ACCESSORS(base_name, Object)
  DECL_ACCESSORS(locale, String)

  // Unicode extension accessors.
  DECL_ACCESSORS(calendar, Object)
  DECL_ACCESSORS(collation, Object)
  DECL_ACCESSORS(numbering_system, Object)

  // CaseFirst: "kf"
  //
  // ecma402 #sec-Intl.Locale.prototype.caseFirst
  enum class CaseFirst {
    UPPER,  // upper case sorts before lower case
    LOWER,  // lower case sorts before upper case
    // (compiler does not like FALSE so we have to name it FALSE_VALUE)
    FALSE_VALUE,  // Turn the feature off
    COUNT
  };
  inline void set_case_first(CaseFirst case_first);
  inline CaseFirst case_first() const;

  // Numeric: 'kn"
  //
  // ecma402 #sec-Intl.Locale.prototype.numeric
  enum class Numeric { NOTSET, TRUE_VALUE, FALSE_VALUE, COUNT };
  inline void set_numeric(Numeric numeric);
  inline Numeric numeric() const;

  // CaseFirst: "hc"
  //
  // ecma402 #sec-Intl.Locale.prototype.hourCycle
  enum class HourCycle {
    H11,  // 12-hour format start with hour 0 and go up to 11.
    H12,  // 12-hour format start with hour 1 and go up to 12.
    H23,  // 24-hour format start with hour 0 and go up to 23.
    H24,  // 24-hour format start with hour 1 and go up to 24.
    COUNT
  };
  inline void set_hour_cycle(HourCycle hour_cycle);
  inline HourCycle hour_cycle() const;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)      \
  V(CaseFirstBits, CaseFirst, 2, _) \
  V(NumericBits, Numeric, 2, _)     \
  V(HourCycleBits, HourCycle, 2, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(CaseFirst::UPPER <= CaseFirstBits::kMax);
  STATIC_ASSERT(CaseFirst::LOWER <= CaseFirstBits::kMax);
  STATIC_ASSERT(CaseFirst::FALSE_VALUE <= CaseFirstBits::kMax);
  STATIC_ASSERT(Numeric::NOTSET <= NumericBits::kMax);
  STATIC_ASSERT(Numeric::FALSE_VALUE <= NumericBits::kMax);
  STATIC_ASSERT(Numeric::TRUE_VALUE <= NumericBits::kMax);
  STATIC_ASSERT(HourCycle::H11 <= HourCycleBits::kMax);
  STATIC_ASSERT(HourCycle::H12 <= HourCycleBits::kMax);
  STATIC_ASSERT(HourCycle::H23 <= HourCycleBits::kMax);
  STATIC_ASSERT(HourCycle::H24 <= HourCycleBits::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  DECL_PRINTER(JSLocale)
  DECL_VERIFIER(JSLocale)

  // Layout description.
  static const int kJSLocaleOffset = JSObject::kHeaderSize;
  // Locale fields.
  static const int kLanguageOffset = kJSLocaleOffset + kPointerSize;
  static const int kScriptOffset = kLanguageOffset + kPointerSize;
  static const int kRegionOffset = kScriptOffset + kPointerSize;
  static const int kBaseNameOffset = kRegionOffset + kPointerSize;
  static const int kLocaleOffset = kBaseNameOffset + kPointerSize;
  // Unicode extension fields.
  static const int kFlagsOffset = kLocaleOffset + kPointerSize;
  static const int kCalendarOffset = kFlagsOffset + kPointerSize;
  static const int kCollationOffset = kCalendarOffset + kPointerSize;
  static const int kNumberingSystemOffset = kCollationOffset + kPointerSize;
  // Final size.
  static const int kSize = kNumberingSystemOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSLocale);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LOCALE_H_
