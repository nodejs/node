// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_PLURAL_RULES_H_
#define V8_OBJECTS_JS_PLURAL_RULES_H_

#include <set>
#include <string>

#include "src/base/bit-field.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class PluralRules;
namespace number {
class LocalizedNumberFormatter;
}  //  namespace number
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSPluralRules : public JSObject {
 public:
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSPluralRules> New(
      Isolate* isolate, Handle<Map> map, Handle<Object> locales,
      Handle<Object> options);

  static Handle<JSObject> ResolvedOptions(Isolate* isolate,
                                          Handle<JSPluralRules> plural_rules);

  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ResolvePlural(
      Isolate* isolate, Handle<JSPluralRules> plural_rules, double number);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  // [[Type]] is one of the values "cardinal" or "ordinal",
  // identifying the plural rules used.
  enum class Type { CARDINAL, ORDINAL };
  inline void set_type(Type type);
  inline Type type() const;

  Handle<String> TypeAsString() const;

  DECL_CAST(JSPluralRules)
  DECL_PRINTER(JSPluralRules)
  DECL_VERIFIER(JSPluralRules)

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _) V(TypeBits, Type, 1, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Type::CARDINAL <= TypeBits::kMax);
  STATIC_ASSERT(Type::ORDINAL <= TypeBits::kMax);

// Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_PLURAL_RULES_FIELDS)

  DECL_ACCESSORS(locale, String)
  DECL_INT_ACCESSORS(flags)
  DECL_ACCESSORS(icu_plural_rules, Managed<icu::PluralRules>)
  DECL_ACCESSORS(icu_number_formatter,
                 Managed<icu::number::LocalizedNumberFormatter>)

  OBJECT_CONSTRUCTORS(JSPluralRules, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PLURAL_RULES_H_
