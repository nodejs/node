// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_SEGMENTS_INL_H_
#define V8_OBJECTS_JS_SEGMENTS_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-segments.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<CppGCManaged<IcuBreakIteratorWithText>>
JSSegments::icu_iterator_with_text() const {
  return Cast<CppGCManaged<IcuBreakIteratorWithText>>(
      icu_iterator_with_text_.load());
}
void JSSegments::set_icu_iterator_with_text(
    Tagged<CppGCManaged<IcuBreakIteratorWithText>> value,
    WriteBarrierMode mode) {
  icu_iterator_with_text_.store(this, value, mode);
}

Tagged<String> JSSegments::raw_string() const { return raw_string_.load(); }
void JSSegments::set_raw_string(Tagged<String> value, WriteBarrierMode mode) {
  raw_string_.store(this, value, mode);
}

JSSegments::Flags JSSegments::flags() const { return flags_.load(); }
void JSSegments::set_flags(Flags value) { flags_.store(value); }

inline void JSSegments::set_granularity(JSSegmenter::Granularity granularity) {
  set_flags(flags().with_granularity(granularity));
}

inline JSSegmenter::Granularity JSSegments::granularity() const {
  return flags().granularity();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENTS_INL_H_
