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

#include "src/base/bit-field.h"
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
class UnlocalizedNumberFormatter;
class LocalizedNumberRangeFormatter;
}  //  namespace number
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-number-format-tq.inc"

class JSNumberFormat
    : public TorqueGeneratedJSNumberFormat<JSNumberFormat, JSObject> {
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

  // ecma402/#sec-formatnumericrange
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> FormatNumericRange(
      Isolate* isolate, Handle<JSNumberFormat> number_format, Handle<Object> x,
      Handle<Object> y);

  // ecma402/#sec-formatnumericrangetoparts
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray> FormatNumericRangeToParts(
      Isolate* isolate, Handle<JSNumberFormat> number_format, Handle<Object> x,
      Handle<Object> y);

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

  enum class ShowTrailingZeros { kShow, kHide };

  static icu::number::UnlocalizedNumberFormatter SetDigitOptionsToFormatter(
      const icu::number::UnlocalizedNumberFormatter& settings,
      const Intl::NumberFormatDigitOptions& digit_options,
      int rounding_increment, ShowTrailingZeros show);

  DECL_PRINTER(JSNumberFormat)

  DECL_ACCESSORS(icu_number_formatter,
                 Managed<icu::number::LocalizedNumberFormatter>)
  DECL_ACCESSORS(icu_number_range_formatter,
                 Managed<icu::number::LocalizedNumberRangeFormatter>)

  TQ_OBJECT_CONSTRUCTORS(JSNumberFormat)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_NUMBER_FORMAT_H_
