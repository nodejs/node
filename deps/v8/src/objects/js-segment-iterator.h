// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_SEGMENT_ITERATOR_H_
#define V8_OBJECTS_JS_SEGMENT_ITERATOR_H_

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

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-segment-iterator-tq.inc"

V8_OBJECT class JSSegmentIterator : public JSObject {
 public:
  // https://tc39.es/ecma402/#sec-CreateSegmentIterator
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSSegmentIterator> Create(
      Isolate* isolate, DirectHandle<String> input_string,
      const icu::BreakIterator& incoming_break_iterator,
      JSSegmenter::Granularity granularity);

  // https://tc39.es/ecma402/#sec-segment-iterator-prototype-next
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSReceiver> Next(
      Isolate* isolate,
      DirectHandle<JSSegmentIterator> segment_iterator_holder);

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

  DECL_PRINTER(JSSegmentIterator)
  DECL_VERIFIER(JSSegmentIterator)

  inline void set_granularity(JSSegmenter::Granularity granularity);
  inline JSSegmenter::Granularity granularity() const;

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_JS_SEGMENT_ITERATOR_FLAGS()

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

inline constexpr int JSSegmentIterator::kIcuIteratorWithTextOffset =
    offsetof(JSSegmentIterator, icu_iterator_with_text_);
inline constexpr int JSSegmentIterator::kRawStringOffset =
    offsetof(JSSegmentIterator, raw_string_);
inline constexpr int JSSegmentIterator::kFlagsOffset =
    offsetof(JSSegmentIterator, flags_);
inline constexpr int JSSegmentIterator::kHeaderSize = sizeof(JSSegmentIterator);

V8_OBJECT class JSSegmentDataObject : public JSObject {
 public:
  inline Tagged<String> segment() const;
  inline void set_segment(Tagged<String> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Number> index() const;
  inline void set_index(Tagged<Number> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> input() const;
  inline void set_input(Tagged<String> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(JSSegmentDataObject)

  // Back-compat offset/size constants.
  static const int kSegmentOffset;
  static const int kIndexOffset;
  static const int kInputOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<String> segment_;
  TaggedMember<Number> index_;
  TaggedMember<String> input_;
} V8_OBJECT_END;

inline constexpr int JSSegmentDataObject::kSegmentOffset =
    offsetof(JSSegmentDataObject, segment_);
inline constexpr int JSSegmentDataObject::kIndexOffset =
    offsetof(JSSegmentDataObject, index_);
inline constexpr int JSSegmentDataObject::kInputOffset =
    offsetof(JSSegmentDataObject, input_);
inline constexpr int JSSegmentDataObject::kHeaderSize =
    sizeof(JSSegmentDataObject);

V8_OBJECT class JSSegmentDataObjectWithIsWordLike : public JSSegmentDataObject {
 public:
  inline Tagged<Boolean> is_word_like() const;
  inline void set_is_word_like(Tagged<Boolean> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(JSSegmentDataObjectWithIsWordLike)

  // Back-compat offset/size constants.
  static const int kIsWordLikeOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<Boolean> is_word_like_;
} V8_OBJECT_END;

inline constexpr int JSSegmentDataObjectWithIsWordLike::kIsWordLikeOffset =
    offsetof(JSSegmentDataObjectWithIsWordLike, is_word_like_);
inline constexpr int JSSegmentDataObjectWithIsWordLike::kHeaderSize =
    sizeof(JSSegmentDataObjectWithIsWordLike);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENT_ITERATOR_H_
