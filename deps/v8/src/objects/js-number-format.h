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

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class UnicodeString;
namespace number {
class LocalizedNumberFormatter;
}  //  namespace number
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSNumberFormat : public JSObject {
 public:
  // ecma402/#sec-initializenumberformat
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSNumberFormat> New(
      Isolate* isolate, Handle<Map> map, Handle<Object> locales,
      Handle<Object> options, const char* service);

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

  // Helper functions shared with JSPluralRules.
  static int32_t MinimumIntegerDigitsFromSkeleton(
      const icu::UnicodeString& skeleton);
  static bool FractionDigitsFromSkeleton(const icu::UnicodeString& skeleton,
                                         int32_t* minimum, int32_t* maximum);
  static bool SignificantDigitsFromSkeleton(const icu::UnicodeString& skeleton,
                                            int32_t* minimum, int32_t* maximum);
  static icu::number::LocalizedNumberFormatter SetDigitOptionsToFormatter(
      const icu::number::LocalizedNumberFormatter& icu_number_formatter,
      const Intl::NumberFormatDigitOptions& digit_options);

  DECL_CAST(JSNumberFormat)
  DECL_PRINTER(JSNumberFormat)
  DECL_VERIFIER(JSNumberFormat)

  // [[Style]] is one of the values "decimal", "percent", "currency",
  // or "unit" identifying the style of the number format.
  // Note: "unit" is added in proposal-unified-intl-numberformat
  enum class Style { DECIMAL, PERCENT, CURRENCY, UNIT };

  inline void set_style(Style style);
  inline Style style() const;

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_NUMBER_FORMAT_FIELDS)

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)            \
  V(StyleBits, Style, 2, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Style::DECIMAL <= StyleBits::kMax);
  STATIC_ASSERT(Style::PERCENT <= StyleBits::kMax);
  STATIC_ASSERT(Style::CURRENCY <= StyleBits::kMax);
  STATIC_ASSERT(Style::UNIT <= StyleBits::kMax);

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
