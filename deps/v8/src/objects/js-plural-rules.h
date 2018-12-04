// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_PLURAL_RULES_H_
#define V8_OBJECTS_JS_PLURAL_RULES_H_

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class PluralRules;
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSPluralRules : public JSObject {
 public:
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSPluralRules> Initialize(
      Isolate* isolate, Handle<JSPluralRules> plural_rules,
      Handle<Object> locales, Handle<Object> options);

  static Handle<JSObject> ResolvedOptions(Isolate* isolate,
                                          Handle<JSPluralRules> plural_rules);

  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ResolvePlural(
      Isolate* isolate, Handle<JSPluralRules> plural_rules, double number);

  DECL_CAST(JSPluralRules)
  DECL_PRINTER(JSPluralRules)
  DECL_VERIFIER(JSPluralRules)

// Layout description.
#define JS_PLURAL_RULES_FIELDS(V)          \
  V(kLocaleOffset, kPointerSize)           \
  /* In the future, this can be an enum,   \
     and not a string. */                  \
  V(kTypeOffset, kPointerSize)             \
  V(kICUPluralRulesOffset, kPointerSize)   \
  V(kICUDecimalFormatOffset, kPointerSize) \
  /* Total size. */                        \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_PLURAL_RULES_FIELDS)
#undef JS_PLURAL_RULES_FIELDS

  DECL_ACCESSORS(locale, String)
  DECL_ACCESSORS(type, String)
  DECL_ACCESSORS(icu_plural_rules, Managed<icu::PluralRules>)
  DECL_ACCESSORS(icu_decimal_format, Managed<icu::DecimalFormat>)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSPluralRules);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PLURAL_RULES_H_
