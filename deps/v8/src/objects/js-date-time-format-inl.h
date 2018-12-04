// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_
#define V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_

#include "src/objects-inl.h"
#include "src/objects/js-date-time-format.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

ACCESSORS(JSDateTimeFormat, icu_locale, Managed<icu::Locale>, kICULocaleOffset);
ACCESSORS(JSDateTimeFormat, icu_simple_date_format,
          Managed<icu::SimpleDateFormat>, kICUSimpleDateFormatOffset)
ACCESSORS(JSDateTimeFormat, bound_format, Object, kBoundFormatOffset);

CAST_ACCESSOR(JSDateTimeFormat);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_
