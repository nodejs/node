// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_SEGMENTS_H_
#define V8_OBJECTS_JS_SEGMENTS_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/base/bit-field.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-segmenter.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-segments-tq.inc"

V8_OBJECT class JSSegments : public JSObject {
 public:
  // https://tc39.es/ecma402/#sec-createsegmentsobject
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSSegments> Create(
      Isolate* isolate, DirectHandle<JSSegmenter> segmenter,
      DirectHandle<String> string);

  // https://tc39.es/ecma402/#sec-%segmentsprototype%.containing
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<Object> Containing(
      Isolate* isolate, DirectHandle<JSSegments> segments_holder, double n);

  // https://tc39.es/ecma402/#sec-createsegmentdataobject
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSSegmentDataObject>
  CreateSegmentDataObject(Isolate* isolate,
                          JSSegmenter::Granularity granularity,
                          icu::BreakIterator* break_iterator,
                          DirectHandle<String> input_string,
                          const icu::UnicodeString& unicode_string,
                          int32_t start_index, int32_t end_index);

  Handle<String> GranularityAsString(Isolate* isolate) const;

  // SegmentIterator accessors.
  inline Tagged<Managed<IcuBreakIteratorWithText>> icu_iterator_with_text()
      const;
  inline void set_icu_iterator_with_text(
      Tagged<Managed<IcuBreakIteratorWithText>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> raw_string() const;
  inline void set_raw_string(Tagged<String> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);

  DECL_PRINTER(JSSegments)
  DECL_VERIFIER(JSSegments)

  inline void set_granularity(JSSegmenter::Granularity granularity);
  inline JSSegmenter::Granularity granularity() const;

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_JS_SEGMENTS_FLAGS()

  static_assert(GranularityBits::is_valid(JSSegmenter::Granularity::GRAPHEME));
  static_assert(GranularityBits::is_valid(JSSegmenter::Granularity::WORD));
  static_assert(GranularityBits::is_valid(JSSegmenter::Granularity::SENTENCE));

  // Back-compat offset/size constants.
  static const int kIcuIteratorWithTextOffset;
  static const int kRawStringOffset;
  static const int kFlagsOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<Foreign> icu_iterator_with_text_;
  TaggedMember<String> raw_string_;
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

inline constexpr int JSSegments::kIcuIteratorWithTextOffset =
    offsetof(JSSegments, icu_iterator_with_text_);
inline constexpr int JSSegments::kRawStringOffset =
    offsetof(JSSegments, raw_string_);
inline constexpr int JSSegments::kFlagsOffset = offsetof(JSSegments, flags_);
inline constexpr int JSSegments::kHeaderSize = sizeof(JSSegments);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENTS_H_
