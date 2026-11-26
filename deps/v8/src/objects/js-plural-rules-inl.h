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
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSPluralRules::locale() const { return locale_.load(); }
void JSPluralRules::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

JSPluralRules::Flags JSPluralRules::flags() const { return flags_.load(); }
void JSPluralRules::set_flags(Flags value) { flags_.store(value); }

Tagged<CppGCManaged<PluralRulesInternal>> JSPluralRules::internal() const {
  return Cast<CppGCManaged<PluralRulesInternal>>(internal_.load());
}
void JSPluralRules::set_internal(
    Tagged<CppGCManaged<PluralRulesInternal>> value, WriteBarrierMode mode) {
  internal_.store(this, value, mode);
}

inline void JSPluralRules::set_type(Type type) {
  DCHECK(Flags::TypeBit::is_valid(type));
  set_flags(flags().with_type(type));
}

inline JSPluralRules::Type JSPluralRules::type() const {
  return flags().type();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PLURAL_RULES_INL_H_
