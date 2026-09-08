// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PLURAL_RULES_INL_H_
#define V8_OBJECTS_JS_PLURAL_RULES_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-plural-rules.h"
// Include the non-inl header before the rest of the headers.

#include "src/api/api-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSPluralRules::locale() const { return locale_.load(); }
void JSPluralRules::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

int JSPluralRules::flags() const { return flags_.load().value(); }
void JSPluralRules::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<Managed<icu::PluralRules>> JSPluralRules::icu_plural_rules() const {
  return Cast<Managed<icu::PluralRules>>(icu_plural_rules_.load());
}
void JSPluralRules::set_icu_plural_rules(
    Tagged<Managed<icu::PluralRules>> value, WriteBarrierMode mode) {
  icu_plural_rules_.store(this, value, mode);
}

Tagged<Managed<icu::number::LocalizedNumberFormatter>>
JSPluralRules::icu_number_formatter() const {
  return Cast<Managed<icu::number::LocalizedNumberFormatter>>(
      icu_number_formatter_.load());
}
void JSPluralRules::set_icu_number_formatter(
    Tagged<Managed<icu::number::LocalizedNumberFormatter>> value,
    WriteBarrierMode mode) {
  icu_number_formatter_.store(this, value, mode);
}

inline void JSPluralRules::set_type(Type type) {
  DCHECK(TypeBit::is_valid(type));
  int hints = flags();
  hints = TypeBit::update(hints, type);
  set_flags(hints);
}

inline JSPluralRules::Type JSPluralRules::type() const {
  return TypeBit::decode(flags());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PLURAL_RULES_INL_H_
