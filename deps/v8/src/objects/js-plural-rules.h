// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PLURAL_RULES_H_
#define V8_OBJECTS_JS_PLURAL_RULES_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

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
class LocalizedNumberRangeFormatter;
}  // namespace number
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-plural-rules-tq.inc"

V8_OBJECT class JSPluralRules : public JSObject {
 public:
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSPluralRules> New(
      Isolate* isolate, DirectHandle<Map> map, DirectHandle<Object> locales,
      DirectHandle<Object> options, const char* method_name);

  static DirectHandle<JSObject> ResolvedOptions(
      Isolate* isolate, DirectHandle<JSPluralRules> plural_rules);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ResolvePlural(
      Isolate* isolate, DirectHandle<JSPluralRules> plural_rules,
      double number);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> ResolvePluralRange(
      Isolate* isolate, DirectHandle<JSPluralRules> plural_rules, double x,
      double y);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  // [[Type]] is one of the values "cardinal" or "ordinal",
  // identifying the plural rules used.
  enum class Type { CARDINAL, ORDINAL };
  inline void set_type(Type type);
  inline Type type() const;

  Handle<String> TypeAsString(Isolate* isolate) const;

  DECL_PRINTER(JSPluralRules)
  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_JS_PLURAL_RULES_FLAGS()

  static_assert(TypeBit::is_valid(Type::CARDINAL));
  static_assert(TypeBit::is_valid(Type::ORDINAL));

  inline Tagged<String> locale() const;
  inline void set_locale(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);

  inline Tagged<Managed<icu::PluralRules>> icu_plural_rules() const;
  inline void set_icu_plural_rules(
      Tagged<Managed<icu::PluralRules>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Managed<icu::number::LocalizedNumberFormatter>>
  icu_number_formatter() const;
  inline void set_icu_number_formatter(
      Tagged<Managed<icu::number::LocalizedNumberFormatter>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(JSPluralRules)

  // Back-compat offset/size constants.
  static const int kLocaleOffset;
  static const int kFlagsOffset;
  static const int kIcuPluralRulesOffset;
  static const int kIcuNumberFormatterOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<String> locale_;
  TaggedMember<Smi> flags_;
  TaggedMember<Foreign> icu_plural_rules_;
  TaggedMember<Foreign> icu_number_formatter_;
} V8_OBJECT_END;

inline constexpr int JSPluralRules::kLocaleOffset =
    offsetof(JSPluralRules, locale_);
inline constexpr int JSPluralRules::kFlagsOffset =
    offsetof(JSPluralRules, flags_);
inline constexpr int JSPluralRules::kIcuPluralRulesOffset =
    offsetof(JSPluralRules, icu_plural_rules_);
inline constexpr int JSPluralRules::kIcuNumberFormatterOffset =
    offsetof(JSPluralRules, icu_number_formatter_);
inline constexpr int JSPluralRules::kHeaderSize = sizeof(JSPluralRules);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PLURAL_RULES_H_
