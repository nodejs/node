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
#include "src/objects/managed.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class Locale;
}

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
  static Handle<String> Maximize(Isolate* isolate, String locale);
  static Handle<String> Minimize(Isolate* isolate, String locale);

  static Handle<Object> Language(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> Script(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> Region(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<String> BaseName(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> Calendar(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> CaseFirst(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> Collation(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> HourCycle(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> Numeric(Isolate* isolate, Handle<JSLocale> locale);
  static Handle<Object> NumberingSystem(Isolate* isolate,
                                        Handle<JSLocale> locale);
  static Handle<String> ToString(Isolate* isolate, Handle<JSLocale> locale);
  static std::string ToString(Handle<JSLocale> locale);

  DECL_CAST(JSLocale)

  DECL_ACCESSORS(icu_locale, Managed<icu::Locale>)

  DECL_PRINTER(JSLocale)
  DECL_VERIFIER(JSLocale)

  // Layout description.
#define JS_LOCALE_FIELDS(V)        \
  V(kICULocaleOffset, kTaggedSize) \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_LOCALE_FIELDS)
#undef JS_LOCALE_FIELDS

  OBJECT_CONSTRUCTORS(JSLocale, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LOCALE_H_
