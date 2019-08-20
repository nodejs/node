// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_SEGMENTER_INL_H_
#define V8_OBJECTS_JS_SEGMENTER_INL_H_

#include "src/objects/js-segmenter.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSSegmenter, JSObject)

// Base segmenter accessors.
ACCESSORS(JSSegmenter, locale, String, kLocaleOffset)
ACCESSORS(JSSegmenter, icu_break_iterator, Managed<icu::BreakIterator>,
          kIcuBreakIteratorOffset)
SMI_ACCESSORS(JSSegmenter, flags, kFlagsOffset)

inline void JSSegmenter::set_granularity(Granularity granularity) {
  DCHECK_GE(GranularityBits::kMax, granularity);
  int hints = flags();
  hints = GranularityBits::update(hints, granularity);
  set_flags(hints);
}

inline JSSegmenter::Granularity JSSegmenter::granularity() const {
  return GranularityBits::decode(flags());
}

CAST_ACCESSOR(JSSegmenter)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_SEGMENTER_INL_H_
