// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_SEGMENTER_H_
#define V8_OBJECTS_JS_SEGMENTER_H_

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/objects/managed.h"
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
  // Initializes segmenter object with properties derived from input
  // locales and options.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSSegmenter> Initialize(
      Isolate* isolate, Handle<JSSegmenter> segmenter_holder,
      Handle<Object> locales, Handle<Object> options);

  V8_WARN_UNUSED_RESULT static Handle<JSObject> ResolvedOptions(
      Isolate* isolate, Handle<JSSegmenter> segmenter_holder);

  Handle<String> LineBreakStyleAsString() const;
  Handle<String> GranularityAsString() const;

  DECL_CAST(JSSegmenter)

  // Segmenter accessors.
  DECL_ACCESSORS(locale, String)

  DECL_ACCESSORS(icu_break_iterator, Managed<icu::BreakIterator>)

  // LineBreakStyle: identifying the style used for line break.
  //
  // ecma402 #sec-segmenter-internal-slots

  enum class LineBreakStyle {
    NOTSET,  // While the granularity is not LINE
    STRICT,  // CSS level 3 line-break=strict, e.g. treat CJ as NS
    NORMAL,  // CSS level 3 line-break=normal, e.g. treat CJ as ID, break before
             // hyphens for ja,zh
    LOOSE,   // CSS level 3 line-break=loose
    COUNT
  };
  inline void set_line_break_style(LineBreakStyle line_break_style);
  inline LineBreakStyle line_break_style() const;

  // Granularity: identifying the segmenter used.
  //
  // ecma402 #sec-segmenter-internal-slots
  enum class Granularity {
    GRAPHEME,  // for character-breaks
    WORD,      // for word-breaks
    SENTENCE,  // for sentence-breaks
    LINE,      // for line-breaks
    COUNT
  };
  inline void set_granularity(Granularity granularity);
  inline Granularity granularity() const;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)                \
  V(LineBreakStyleBits, LineBreakStyle, 3, _) \
  V(GranularityBits, Granularity, 3, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(LineBreakStyle::NOTSET <= LineBreakStyleBits::kMax);
  STATIC_ASSERT(LineBreakStyle::STRICT <= LineBreakStyleBits::kMax);
  STATIC_ASSERT(LineBreakStyle::NORMAL <= LineBreakStyleBits::kMax);
  STATIC_ASSERT(LineBreakStyle::LOOSE <= LineBreakStyleBits::kMax);
  STATIC_ASSERT(Granularity::GRAPHEME <= GranularityBits::kMax);
  STATIC_ASSERT(Granularity::WORD <= GranularityBits::kMax);
  STATIC_ASSERT(Granularity::SENTENCE <= GranularityBits::kMax);
  STATIC_ASSERT(Granularity::LINE <= GranularityBits::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  DECL_PRINTER(JSSegmenter)
  DECL_VERIFIER(JSSegmenter)

  // Layout description.
  static const int kJSSegmenterOffset = JSObject::kHeaderSize;
  static const int kLocaleOffset = kJSSegmenterOffset + kPointerSize;
  static const int kICUBreakIteratorOffset = kLocaleOffset + kPointerSize;
  static const int kFlagsOffset = kICUBreakIteratorOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

 private:
  static LineBreakStyle GetLineBreakStyle(const char* str);
  static Granularity GetGranularity(const char* str);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSSegmenter);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENTER_H_
