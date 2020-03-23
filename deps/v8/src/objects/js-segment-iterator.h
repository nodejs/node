// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_SEGMENT_ITERATOR_H_
#define V8_OBJECTS_JS_SEGMENT_ITERATOR_H_

#include "src/base/bit-field.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/js-segmenter.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
class UnicodeString;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSSegmentIterator : public JSObject {
 public:
  // ecma402 #sec-CreateSegmentIterator
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSSegmentIterator> Create(
      Isolate* isolate, icu::BreakIterator* icu_break_iterator,
      JSSegmenter::Granularity granularity, Handle<String> string);

  // ecma402 #sec-segment-iterator-prototype-next
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> Next(
      Isolate* isolate, Handle<JSSegmentIterator> segment_iterator_holder);

  // ecma402 #sec-segment-iterator-prototype-following
  static Maybe<bool> Following(
      Isolate* isolate, Handle<JSSegmentIterator> segment_iterator_holder,
      Handle<Object> from);

  // ecma402 #sec-segment-iterator-prototype-preceding
  static Maybe<bool> Preceding(
      Isolate* isolate, Handle<JSSegmentIterator> segment_iterator_holder,
      Handle<Object> from);

  // ecma402 #sec-segment-iterator-prototype-index
  static Handle<Object> Index(
      Isolate* isolate, Handle<JSSegmentIterator> segment_iterator_holder);

  Handle<String> GranularityAsString() const;

  DECL_BOOLEAN_ACCESSORS(is_break_type_set)

  // ecma402 #sec-segment-iterator-prototype-breakType
  Handle<Object> BreakType() const;

  V8_WARN_UNUSED_RESULT MaybeHandle<String> GetSegment(Isolate* isolate,
                                                       int32_t start,
                                                       int32_t end) const;

  DECL_CAST(JSSegmentIterator)

  // SegmentIterator accessors.
  DECL_ACCESSORS(icu_break_iterator, Managed<icu::BreakIterator>)
  DECL_ACCESSORS(unicode_string, Managed<icu::UnicodeString>)

  DECL_PRINTER(JSSegmentIterator)
  DECL_VERIFIER(JSSegmentIterator)

  inline void set_granularity(JSSegmenter::Granularity granularity);
  inline JSSegmenter::Granularity granularity() const;

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_JS_SEGMENT_ITERATOR_FLAGS()

  STATIC_ASSERT(JSSegmenter::Granularity::GRAPHEME <= GranularityBits::kMax);
  STATIC_ASSERT(JSSegmenter::Granularity::WORD <= GranularityBits::kMax);
  STATIC_ASSERT(JSSegmenter::Granularity::SENTENCE <= GranularityBits::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

// Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_SEGMENT_ITERATOR_FIELDS)

  OBJECT_CONSTRUCTORS(JSSegmentIterator, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENT_ITERATOR_H_
