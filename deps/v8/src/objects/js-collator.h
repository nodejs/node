// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLATOR_H_
#define V8_OBJECTS_JS_COLLATOR_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <set>
#include <string>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-objects.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class Collator;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-collator-tq.inc"

V8_OBJECT class JSCollator : public JSObject {
 public:
  // https://tc39.es/ecma402/#sec-initializecollator
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<JSCollator> New(
      Isolate* isolate, DirectHandle<Map> map, DirectHandle<Object> locales,
      DirectHandle<Object> options, const char* service);

  // https://tc39.es/ecma402/#sec-intl.collator.prototype.resolvedoptions
  static DirectHandle<JSObject> ResolvedOptions(
      Isolate* isolate, DirectHandle<JSCollator> collator);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  inline Tagged<Managed<icu::Collator>> icu_collator() const;
  inline void set_icu_collator(Tagged<Managed<icu::Collator>> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, JSFunction>> bound_compare() const;
  inline void set_bound_compare(Tagged<UnionOf<Undefined, JSFunction>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> locale() const;
  inline void set_locale(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSCollator)
  DECL_VERIFIER(JSCollator)

  // Back-compat offset/size constants.
  static const int kIcuCollatorOffset;
  static const int kBoundCompareOffset;
  static const int kLocaleOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<Foreign> icu_collator_;
  TaggedMember<UnionOf<Undefined, JSFunction>> bound_compare_;
  TaggedMember<String> locale_;
} V8_OBJECT_END;

inline constexpr int JSCollator::kIcuCollatorOffset =
    offsetof(JSCollator, icu_collator_);
inline constexpr int JSCollator::kBoundCompareOffset =
    offsetof(JSCollator, bound_compare_);
inline constexpr int JSCollator::kLocaleOffset = offsetof(JSCollator, locale_);
inline constexpr int JSCollator::kHeaderSize = sizeof(JSCollator);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLATOR_H_
