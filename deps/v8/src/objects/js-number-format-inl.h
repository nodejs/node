// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_NUMBER_FORMAT_INL_H_
#define V8_OBJECTS_JS_NUMBER_FORMAT_INL_H_

#include "src/objects/js-number-format.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSNumberFormat, JSObject)

ACCESSORS(JSNumberFormat, locale, String, kLocaleOffset)
ACCESSORS(JSNumberFormat, icu_number_formatter,
          Managed<icu::number::LocalizedNumberFormatter>,
          kIcuNumberFormatterOffset)
ACCESSORS(JSNumberFormat, bound_format, Object, kBoundFormatOffset)

// Currenct ECMA 402 spec mandate to record (Min|Max)imumFractionDigits
// uncondictionally while the unified number proposal eventually will only
// record either (Min|Max)imumFractionDigits or (Min|Max)imumSignaficantDigits
// Since LocalizedNumberFormatter can only remember one set, and during
// 2019-1-17 ECMA402 meeting that the committee decide not to take a PR to
// address that prior to the unified number proposal, we have to add these two
// 5 bits int into flags to remember the (Min|Max)imumFractionDigits while
// (Min|Max)imumSignaficantDigits is present.
// TODO(ftang) remove the following once we ship int-number-format-unified
//  * SMI_ACCESSORS of flags
//  * Four inline functions: (set_)?(min|max)imum_fraction_digits

SMI_ACCESSORS(JSNumberFormat, flags, kFlagsOffset)

inline int JSNumberFormat::minimum_fraction_digits() const {
  return MinimumFractionDigitsBits::decode(flags());
}

inline void JSNumberFormat::set_minimum_fraction_digits(int digits) {
  DCHECK_GE(MinimumFractionDigitsBits::kMax, digits);
  DCHECK_LE(0, digits);
  DCHECK_GE(20, digits);
  int hints = flags();
  hints = MinimumFractionDigitsBits::update(hints, digits);
  set_flags(hints);
}

inline int JSNumberFormat::maximum_fraction_digits() const {
  return MaximumFractionDigitsBits::decode(flags());
}

inline void JSNumberFormat::set_maximum_fraction_digits(int digits) {
  DCHECK_GE(MaximumFractionDigitsBits::kMax, digits);
  DCHECK_LE(0, digits);
  DCHECK_GE(20, digits);
  int hints = flags();
  hints = MaximumFractionDigitsBits::update(hints, digits);
  set_flags(hints);
}

inline void JSNumberFormat::set_style(Style style) {
  DCHECK_GE(StyleBits::kMax, style);
  int hints = flags();
  hints = StyleBits::update(hints, style);
  set_flags(hints);
}

inline JSNumberFormat::Style JSNumberFormat::style() const {
  return StyleBits::decode(flags());
}

CAST_ACCESSOR(JSNumberFormat)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_NUMBER_FORMAT_INL_H_
