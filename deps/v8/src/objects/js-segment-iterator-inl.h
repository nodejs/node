// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_SEGMENT_ITERATOR_INL_H_
#define V8_OBJECTS_JS_SEGMENT_ITERATOR_INL_H_

#include "src/objects-inl.h"
#include "src/objects/js-segment-iterator.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSSegmentIterator, JSObject)

// Base segment iterator accessors.
ACCESSORS(JSSegmentIterator, icu_break_iterator, Managed<icu::BreakIterator>,
          kICUBreakIteratorOffset)
ACCESSORS(JSSegmentIterator, unicode_string, Managed<icu::UnicodeString>,
          kUnicodeStringOffset)

BIT_FIELD_ACCESSORS(JSSegmentIterator, flags, is_break_type_set,
                    JSSegmentIterator::BreakTypeSetBits)

SMI_ACCESSORS(JSSegmentIterator, flags, kFlagsOffset)

CAST_ACCESSOR(JSSegmentIterator);

inline void JSSegmentIterator::set_granularity(
    JSSegmenter::Granularity granularity) {
  DCHECK_GT(JSSegmenter::Granularity::COUNT, granularity);
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
