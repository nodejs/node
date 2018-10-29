// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_LOCALE_INL_H_
#define V8_OBJECTS_JS_LOCALE_INL_H_

#include "src/api-inl.h"
#include "src/objects-inl.h"
#include "src/objects/js-locale.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Base locale accessors.
ACCESSORS(JSLocale, language, Object, kLanguageOffset);
ACCESSORS(JSLocale, script, Object, kScriptOffset);
ACCESSORS(JSLocale, region, Object, kRegionOffset);
ACCESSORS(JSLocale, base_name, Object, kBaseNameOffset);
ACCESSORS(JSLocale, locale, String, kLocaleOffset);

// Unicode extension accessors.
ACCESSORS(JSLocale, calendar, Object, kCalendarOffset);
ACCESSORS(JSLocale, collation, Object, kCollationOffset);
ACCESSORS(JSLocale, numbering_system, Object, kNumberingSystemOffset);
SMI_ACCESSORS(JSLocale, flags, kFlagsOffset)

CAST_ACCESSOR(JSLocale);

inline void JSLocale::set_case_first(CaseFirst case_first) {
  DCHECK_GT(CaseFirst::COUNT, case_first);
  int hints = flags();
  hints = CaseFirstBits::update(hints, case_first);
  set_flags(hints);
}

inline JSLocale::CaseFirst JSLocale::case_first() const {
  return CaseFirstBits::decode(flags());
}

inline void JSLocale::set_hour_cycle(HourCycle hour_cycle) {
  DCHECK_GT(HourCycle::COUNT, hour_cycle);
  int hints = flags();
  hints = HourCycleBits::update(hints, hour_cycle);
  set_flags(hints);
}

inline JSLocale::HourCycle JSLocale::hour_cycle() const {
  return HourCycleBits::decode(flags());
}

inline void JSLocale::set_numeric(Numeric numeric) {
  DCHECK_GT(Numeric::COUNT, numeric);
  int hints = flags();
  hints = NumericBits::update(hints, numeric);
  set_flags(hints);
}

inline JSLocale::Numeric JSLocale::numeric() const {
  return NumericBits::decode(flags());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LOCALE_INL_H_
