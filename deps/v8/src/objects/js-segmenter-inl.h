// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_SEGMENTER_INL_H_
#define V8_OBJECTS_JS_SEGMENTER_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-segmenter.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSSegmenter::locale() const { return locale_.load(); }
void JSSegmenter::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

Tagged<Managed<icu::BreakIterator>> JSSegmenter::icu_break_iterator() const {
  return Cast<Managed<icu::BreakIterator>>(icu_break_iterator_.load());
}
void JSSegmenter::set_icu_break_iterator(
    Tagged<Managed<icu::BreakIterator>> value, WriteBarrierMode mode) {
  icu_break_iterator_.store(this, value, mode);
}

JSSegmenter::Flags JSSegmenter::flags() const { return flags_.load(); }
void JSSegmenter::set_flags(Flags value) { flags_.store(value); }

inline void JSSegmenter::set_granularity(Granularity granularity) {
  set_flags(flags().with_granularity(granularity));
}

inline JSSegmenter::Granularity JSSegmenter::granularity() const {
  return flags().granularity();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENTER_INL_H_
