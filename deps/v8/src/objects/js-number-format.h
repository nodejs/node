// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_NUMBER_FORMAT_H_
#define V8_OBJECTS_JS_NUMBER_FORMAT_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

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
class Formattable;
class UnicodeString;
namespace number {
class FormattedNumber;
class FormattedNumberRange;
class LocalizedNumberFormatter;
class LocalizedNumberRangeFormatter;
class UnlocalizedNumberFormatter;
}  //  namespace number
}  //  namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-number-format-tq.inc"

V8_OBJECT class JSNumberFormat : public JSObject {
 public:
  // https://tc39.es/ecma402/#sec-initializenumberformat
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSNumberFormat> New(
      Isolate* isolate, DirectHandle<Map> map, DirectHandle<Object> locales,
      DirectHandle<Object> options, const char* service);

  // https://tc39.es/ecma402/#sec-unwrapnumberformat
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSNumberFormat>
  UnwrapNumberFormat(Isolate* isolate, DirectHandle<JSReceiver> format_holder);

  // https://tc39.es/ecma262/#sec-number-format-functions
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> NumberFormatFunction(
      Isolate* isolate, DirectHandle<JSNumberFormat> number_format,
      Handle<Object> numeric_obj);

  // https://tc39.es/ecma402/#sec-intl.numberformat.prototype.resolvedoptions
  static DirectHandle<JSObject> ResolvedOptions(
      Isolate* isolate, DirectHandle<JSNumberFormat> number_format);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSArray> FormatToParts(
      Isolate* isolate, DirectHandle<JSNumberFormat> number_format,
      Handle<Object> numeric_obj);

  // https://tc39.es/ecma402/#sec-formatnumericrange
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> FormatNumericRange(
      Isolate* isolate, DirectHandle<JSNumberFormat> number_format,
      Handle<Object> x, Handle<Object> y);

  // https://tc39.es/ecma402/#sec-formatnumericrangetoparts
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSArray>
  FormatNumericRangeToParts(Isolate* isolate,
                            DirectHandle<JSNumberFormat> number_format,
                            Handle<Object> x, Handle<Object> y);

  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> FormatNumeric(
      Isolate* isolate, const icu::number::LocalizedNumberFormatter* lfmt,
      Handle<Object> numeric_obj);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  // Helper functions shared with JSPluralRules.
  static int32_t MinimumIntegerDigitsFromSkeleton(
      const icu::UnicodeString& skeleton);
  static bool FractionDigitsFromSkeleton(const icu::UnicodeString& skeleton,
                                         int32_t* minimum, int32_t* maximum);
  static bool SignificantDigitsFromSkeleton(const icu::UnicodeString& skeleton,
                                            int32_t* minimum, int32_t* maximum);

  static DirectHandle<String> RoundingModeString(
      Isolate* isolate, const icu::UnicodeString& skeleton);
  static DirectHandle<String> RoundingPriorityString(
      Isolate* isolate, const icu::UnicodeString& skeleton);
  static DirectHandle<String> TrailingZeroDisplayString(
      Isolate* isolate, const icu::UnicodeString& skeleton);
  static DirectHandle<Object> RoundingIncrement(
      Isolate* isolate, const icu::UnicodeString& skeleton);

  enum class ShowTrailingZeros { kShow, kHide };

  static icu::number::UnlocalizedNumberFormatter SetDigitOptionsToFormatter(
      const icu::number::UnlocalizedNumberFormatter& settings,
      const Intl::NumberFormatDigitOptions& digit_options);

  static const icu::UnicodeString NumberingSystemFromSkeleton(
      const icu::UnicodeString& skeleton);

  V8_WARN_UNUSED_RESULT static Maybe<icu::number::LocalizedNumberRangeFormatter>
  GetRangeFormatter(
      Isolate* isolate, Tagged<String> locale,
      const icu::number::LocalizedNumberFormatter& number_formatter);

  DECL_PRINTER(JSNumberFormat)
  DECL_VERIFIER(JSNumberFormat)

  inline Tagged<String> locale() const;
  inline void set_locale(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Managed<icu::number::LocalizedNumberFormatter>>
  icu_number_formatter() const;
  inline void set_icu_number_formatter(
      Tagged<Managed<icu::number::LocalizedNumberFormatter>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSFunction, Undefined>> bound_format() const;
  inline void set_bound_format(Tagged<UnionOf<JSFunction, Undefined>> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static const int kHeaderSize;

 public:
  TaggedMember<String> locale_;
  TaggedMember<Foreign> icu_number_formatter_;
  TaggedMember<UnionOf<JSFunction, Undefined>> bound_format_;
} V8_OBJECT_END;

inline constexpr int JSNumberFormat::kHeaderSize = sizeof(JSNumberFormat);

// IntlMathematicalValue is designed only to be used as part of
// JSNumberFormat and can only be allocate on the stack. We place this class in
// the header so we can write unit test code for it. Please do NOT use this
// class outside JSNumberFormat implementation.
class V8_NODISCARD IntlMathematicalValue {
 public:
  IntlMathematicalValue() : approx_(0) {}
  V8_EXPORT_PRIVATE bool IsNaN() const;

  V8_EXPORT_PRIVATE static Maybe<IntlMathematicalValue> From(
      Isolate* isolate, Handle<Object> value);

  static Maybe<icu::number::FormattedNumber> FormatNumeric(
      Isolate* isolate, const icu::number::LocalizedNumberFormatter* lfmt,
      const IntlMathematicalValue& x);

  static Maybe<icu::number::FormattedNumberRange> FormatRange(
      Isolate* isolate,
      const icu::number::LocalizedNumberRangeFormatter& number_range_format,
      const IntlMathematicalValue& x, const IntlMathematicalValue& y);

 private:
  double approx_;
  Handle<Object> value_;  // Number, BigInt or String
  Maybe<icu::Formattable> ToFormattable(Isolate* isolate) const;
  MaybeHandle<String> ToString(Isolate* isolate) const;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_NUMBER_FORMAT_H_
