// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_LOCALE_H_
#define V8_OBJECTS_JS_LOCALE_H_

#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class Locale;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-locale-tq.inc"

class JSLocale : public TorqueGeneratedJSLocale<JSLocale, JSObject> {
 public:
  // Creates locale object with properties derived from input locale string
  // and options.
  static MaybeHandle<JSLocale> New(Isolate* isolate, Handle<Map> map,
                                   Handle<String> locale,
                                   Handle<JSReceiver> options);

  static MaybeHandle<JSLocale> Maximize(Isolate* isolate,
                                        Handle<JSLocale> locale);
  static MaybeHandle<JSLocale> Minimize(Isolate* isolate,
                                        Handle<JSLocale> locale);

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

  // Help function to validate locale by other Intl objects.
  static bool StartsWithUnicodeLanguageId(const std::string& value);

  // Help function to check well-formed
  // "(3*8alphanum) *("-" (3*8alphanum)) sequence" sequence
  static bool Is38AlphaNumList(const std::string& value);

  // Help function to check well-formed "3alpha"
  static bool Is3Alpha(const std::string& value);

  DECL_ACCESSORS(icu_locale, Managed<icu::Locale>)

  DECL_PRINTER(JSLocale)

  TQ_OBJECT_CONSTRUCTORS(JSLocale)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LOCALE_H_
