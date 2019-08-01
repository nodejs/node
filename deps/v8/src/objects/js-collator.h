// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_COLLATOR_H_
#define V8_OBJECTS_JS_COLLATOR_H_

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
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSCollator : public JSObject {
 public:
  // ecma402/#sec-initializecollator
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSCollator> Initialize(
      Isolate* isolate, Handle<JSCollator> collator, Handle<Object> locales,
      Handle<Object> options);

  // ecma402/#sec-intl.collator.prototype.resolvedoptions
  static Handle<JSObject> ResolvedOptions(Isolate* isolate,
                                          Handle<JSCollator> collator);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  DECL_CAST(JSCollator)
  DECL_PRINTER(JSCollator)
  DECL_VERIFIER(JSCollator)

// Layout description.
#define JS_COLLATOR_FIELDS(V)         \
  V(kICUCollatorOffset, kTaggedSize)  \
  V(kBoundCompareOffset, kTaggedSize) \
  /* Total size. */                   \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_COLLATOR_FIELDS)
#undef JS_COLLATOR_FIELDS

  DECL_ACCESSORS(icu_collator, Managed<icu::Collator>)
  DECL_ACCESSORS(bound_compare, Object)

  OBJECT_CONSTRUCTORS(JSCollator, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLATOR_H_
