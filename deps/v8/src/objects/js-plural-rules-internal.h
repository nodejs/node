// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PLURAL_RULES_INTERNAL_H_
#define V8_OBJECTS_JS_PLURAL_RULES_INTERNAL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <memory>

#include "include/v8-internal.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-plural-rules.h"

namespace v8 {
namespace internal {

// Abstract backend interface for Intl.PluralRules (ICU4C vs ICU4X).
// The active backend is selected once at construction time in
// PluralRulesInternal::Create so callers of JSPluralRules do not need to
// branch on backend flags.
class PluralRulesInternal {
 public:
  static constexpr ManagedTypeId kTypeID =
      ManagedTypeId::kPluralRulesInternal;

  PluralRulesInternal() = default;
  virtual ~PluralRulesInternal() = default;

  static std::unique_ptr<PluralRulesInternal> Create(
      Isolate* isolate, const Intl::ResolvedLocale& resolved_locale,
      JSPluralRules::Type type, Intl::Notation notation,
      Intl::CompactDisplay compact_display,
      const Intl::NumberFormatDigitOptions& digit_options);

  virtual MaybeDirectHandle<String> Select(Isolate* isolate,
                                           double number) const = 0;
  virtual MaybeDirectHandle<String> SelectRange(Isolate* isolate,
                                                Tagged<String> locale_str,
                                                double x, double y) const = 0;
  virtual void PopulateResolvedOptions(
      Isolate* isolate, DirectHandle<JSObject> options) const = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_PLURAL_RULES_INTERNAL_H_
