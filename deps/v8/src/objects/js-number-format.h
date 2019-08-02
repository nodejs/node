// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_NUMBER_FORMAT_H_
#define V8_OBJECTS_JS_NUMBER_FORMAT_H_

#include <set>
#include <string>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "unicode/numberformatter.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class NumberFormat;
class UnicodeString;
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSNumberFormat : public JSObject {
 public:
  // ecma402/#sec-initializenumberformat
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSNumberFormat> Initialize(
      Isolate* isolate, Handle<JSNumberFormat> number_format,
      Handle<Object> locales, Handle<Object> options);

  // ecma402/#sec-unwrapnumberformat
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSNumberFormat> UnwrapNumberFormat(
      Isolate* isolate, Handle<JSReceiver> format_holder);

  // ecma402/#sec-intl.numberformat.prototype.resolvedoptions
  static Handle<JSObject> ResolvedOptions(Isolate* isolate,
                                          Handle<JSNumberFormat> number_format);

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray> FormatToParts(
      Isolate* isolate, Handle<JSNumberFormat> number_format,
      Handle<Object> numeric_obj);

  V8_WARN_UNUSED_RESULT static MaybeHandle<String> FormatNumeric(
      Isolate* isolate,
      const icu::number::LocalizedNumberFormatter& number_format,
      Handle<Object> numeric_obj);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  DECL_CAST(JSNumberFormat)
  DECL_PRINTER(JSNumberFormat)
  DECL_VERIFIER(JSNumberFormat)

  // Current ECMA 402 spec mandates to record (Min|Max)imumFractionDigits
  // unconditionally while the unified number proposal eventually will only
  // record either (Min|Max)imumFractionDigits or (Min|Max)imumSignaficantDigits
  // Since LocalizedNumberFormatter can only remember one set, and during
  // 2019-1-17 ECMA402 meeting that the committee decide not to take a PR to
  // address that prior to the unified number proposal, we have to add these two
  // 5 bits int into flags to remember the (Min|Max)imumFractionDigits while
  // (Min|Max)imumSignaficantDigits is present.
  // TODO(ftang) remove the following once we ship int-number-format-unified
  //  * Four inline functions: (set_)?(min|max)imum_fraction_digits
  //  * kFlagsOffset
  //  * #define FLAGS_BIT_FIELDS
  //  * DECL_INT_ACCESSORS(flags)

  inline int minimum_fraction_digits() const;
  inline void set_minimum_fraction_digits(int digits);

  inline int maximum_fraction_digits() const;
  inline void set_maximum_fraction_digits(int digits);

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSNUMBER_FORMAT_FIELDS)

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)            \
  V(MinimumFractionDigitsBits, int, 5, _) \
  V(MaximumFractionDigitsBits, int, 5, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(20 <= MinimumFractionDigitsBits::kMax);
  STATIC_ASSERT(20 <= MaximumFractionDigitsBits::kMax);

  DECL_ACCESSORS(locale, String)
  DECL_ACCESSORS(icu_number_formatter,
                 Managed<icu::number::LocalizedNumberFormatter>)
  DECL_ACCESSORS(bound_format, Object)
  DECL_INT_ACCESSORS(flags)

  OBJECT_CONSTRUCTORS(JSNumberFormat, JSObject);
};

struct NumberFormatSpan {
  int32_t field_id;
  int32_t begin_pos;
  int32_t end_pos;

  NumberFormatSpan() = default;
  NumberFormatSpan(int32_t field_id, int32_t begin_pos, int32_t end_pos)
      : field_id(field_id), begin_pos(begin_pos), end_pos(end_pos) {}
};

V8_EXPORT_PRIVATE std::vector<NumberFormatSpan> FlattenRegionsToParts(
    std::vector<NumberFormatSpan>* regions);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_NUMBER_FORMAT_H_
