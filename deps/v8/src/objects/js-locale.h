// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_LOCALE_H_
#define V8_OBJECTS_JS_LOCALE_H_

#include "src/api.h"
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
  static MaybeHandle<JSLocale> InitializeLocale(Isolate* isolate,
                                                Handle<JSLocale> locale_holder,
                                                Handle<String> locale,
                                                Handle<JSReceiver> options);

  DECL_CAST(JSLocale)

  // Locale accessors.
  DECL_ACCESSORS(language, Object)
  DECL_ACCESSORS(script, Object)
  DECL_ACCESSORS(region, Object)
  DECL_ACCESSORS(base_name, Object)
  DECL_ACCESSORS(locale, String)

  // Unicode extension accessors.
  DECL_ACCESSORS(calendar, Object)
  DECL_ACCESSORS(case_first, Object)
  DECL_ACCESSORS(collation, Object)
  DECL_ACCESSORS(hour_cycle, Object)
  DECL_ACCESSORS(numeric, Object)
  DECL_ACCESSORS(numbering_system, Object)

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
  static const int kCalendarOffset = kLocaleOffset + kPointerSize;
  static const int kCaseFirstOffset = kCalendarOffset + kPointerSize;
  static const int kCollationOffset = kCaseFirstOffset + kPointerSize;
  static const int kHourCycleOffset = kCollationOffset + kPointerSize;
  static const int kNumericOffset = kHourCycleOffset + kPointerSize;
  static const int kNumberingSystemOffset = kNumericOffset + kPointerSize;
  // Final size.
  static const int kSize = kNumberingSystemOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSLocale);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_LOCALE_H_
