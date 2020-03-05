// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_SEGMENTER_H_
#define V8_OBJECTS_JS_SEGMENTER_H_

#include <set>
#include <string>

#include "src/base/bit-field.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
}

namespace v8 {
namespace internal {

class JSSegmenter : public JSObject {
 public:
  // Creates segmenter object with properties derived from input locales and
  // options.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSSegmenter> New(
      Isolate* isolate, Handle<Map> map, Handle<Object> locales,
      Handle<Object> options);

  V8_WARN_UNUSED_RESULT static Handle<JSObject> ResolvedOptions(
      Isolate* isolate, Handle<JSSegmenter> segmenter_holder);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  Handle<String> GranularityAsString() const;

  DECL_CAST(JSSegmenter)

  // Segmenter accessors.
  DECL_ACCESSORS(locale, String)

  DECL_ACCESSORS(icu_break_iterator, Managed<icu::BreakIterator>)

  // Granularity: identifying the segmenter used.
  //
  // ecma402 #sec-segmenter-internal-slots
  enum class Granularity {
    GRAPHEME,  // for character-breaks
    WORD,      // for word-breaks
    SENTENCE   // for sentence-breaks
  };
  inline void set_granularity(Granularity granularity);
  inline Granularity granularity() const;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _) V(GranularityBits, Granularity, 2, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Granularity::GRAPHEME <= GranularityBits::kMax);
  STATIC_ASSERT(Granularity::WORD <= GranularityBits::kMax);
  STATIC_ASSERT(Granularity::SENTENCE <= GranularityBits::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  DECL_PRINTER(JSSegmenter)
  DECL_VERIFIER(JSSegmenter)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_SEGMENTER_FIELDS)

 private:
  static Granularity GetGranularity(const char* str);

  OBJECT_CONSTRUCTORS(JSSegmenter, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENTER_H_
