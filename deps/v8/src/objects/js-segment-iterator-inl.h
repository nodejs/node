// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_SEGMENT_ITERATOR_INL_H_
#define V8_OBJECTS_JS_SEGMENT_ITERATOR_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-segment-iterator.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<Managed<IcuBreakIteratorWithText>>
JSSegmentIterator::icu_iterator_with_text() const {
  return Cast<Managed<IcuBreakIteratorWithText>>(
      icu_iterator_with_text_.load());
}
void JSSegmentIterator::set_icu_iterator_with_text(
    Tagged<Managed<IcuBreakIteratorWithText>> value, WriteBarrierMode mode) {
  icu_iterator_with_text_.store(this, value, mode);
}

Tagged<String> JSSegmentIterator::raw_string() const {
  return raw_string_.load();
}
void JSSegmentIterator::set_raw_string(Tagged<String> value,
                                       WriteBarrierMode mode) {
  raw_string_.store(this, value, mode);
}

int JSSegmentIterator::flags() const { return flags_.load().value(); }
void JSSegmentIterator::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<String> JSSegmentDataObject::segment() const { return segment_.load(); }
void JSSegmentDataObject::set_segment(Tagged<String> value,
                                      WriteBarrierMode mode) {
  segment_.store(this, value, mode);
}

Tagged<Number> JSSegmentDataObject::index() const { return index_.load(); }
void JSSegmentDataObject::set_index(Tagged<Number> value,
                                    WriteBarrierMode mode) {
  index_.store(this, value, mode);
}

Tagged<String> JSSegmentDataObject::input() const { return input_.load(); }
void JSSegmentDataObject::set_input(Tagged<String> value,
                                    WriteBarrierMode mode) {
  input_.store(this, value, mode);
}

Tagged<Boolean> JSSegmentDataObjectWithIsWordLike::is_word_like() const {
  return is_word_like_.load();
}
void JSSegmentDataObjectWithIsWordLike::set_is_word_like(
    Tagged<Boolean> value, WriteBarrierMode mode) {
  is_word_like_.store(this, value, mode);
}

inline void JSSegmentIterator::set_granularity(
    JSSegmenter::Granularity granularity) {
  DCHECK(GranularityBits::is_valid(granularity));
  int hints = flags();
  hints = GranularityBits::update(hints, granularity);
  set_flags(hints);
}

inline JSSegmenter::Granularity JSSegmentIterator::granularity() const {
  return GranularityBits::decode(flags());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENT_ITERATOR_INL_H_
