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

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class NumberFormat;
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

  // A utility function used by the above JSNumberFormat::FormatToParts()
  // and JSRelativeTimeFormat::FormatToParts().
  // Format the number by using the icu::NumberFormat to get the field
  // information. It add an object into the result array, starting from the
  // start_index and return the total number of elements in the result array.
  // For each object added as element, it set the substring of the field as
  // "value", the field type as "type". If the unit is not null, it also set
  // unit as "unit" to each added object.
  V8_WARN_UNUSED_RESULT static Maybe<int> FormatToParts(
      Isolate* isolate, Handle<JSArray> result, int start_index,
      const icu::NumberFormat& fmt, Handle<Object> numeric_obj,
      Handle<String> unit);

  V8_WARN_UNUSED_RESULT static MaybeHandle<String> FormatNumeric(
      Isolate* isolate, const icu::NumberFormat& number_format,
      Handle<Object> numeric_obj);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  Handle<String> StyleAsString() const;
  Handle<String> CurrencyDisplayAsString() const;

  DECL_CAST(JSNumberFormat)
  DECL_PRINTER(JSNumberFormat)
  DECL_VERIFIER(JSNumberFormat)

  // [[Style]] is one of the values "decimal", "percent" or "currency",
  // identifying the style of the number format.
  enum class Style {
    DECIMAL,
    PERCENT,
    CURRENCY,

    COUNT
  };
  inline void set_style(Style style);
  inline Style style() const;

  // [[CurrencyDisplay]] is one of the values "code", "symbol" or "name",
  // identifying the display of the currency number format.
  enum class CurrencyDisplay {
    CODE,
    SYMBOL,
    NAME,

    COUNT
  };
  inline void set_currency_display(CurrencyDisplay currency_display);
  inline CurrencyDisplay currency_display() const;

// Layout description.
#define JS_NUMBER_FORMAT_FIELDS(V)       \
  V(kLocaleOffset, kTaggedSize)          \
  V(kICUNumberFormatOffset, kTaggedSize) \
  V(kBoundFormatOffset, kTaggedSize)     \
  V(kFlagsOffset, kTaggedSize)           \
  /* Total size. */                      \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_NUMBER_FORMAT_FIELDS)
#undef JS_NUMBER_FORMAT_FIELDS

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _) \
  V(StyleBits, Style, 2, _)    \
  V(CurrencyDisplayBits, CurrencyDisplay, 2, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Style::DECIMAL <= StyleBits::kMax);
  STATIC_ASSERT(Style::PERCENT <= StyleBits::kMax);
  STATIC_ASSERT(Style::CURRENCY <= StyleBits::kMax);

  STATIC_ASSERT(CurrencyDisplay::CODE <= CurrencyDisplayBits::kMax);
  STATIC_ASSERT(CurrencyDisplay::SYMBOL <= CurrencyDisplayBits::kMax);
  STATIC_ASSERT(CurrencyDisplay::NAME <= CurrencyDisplayBits::kMax);

  DECL_ACCESSORS(locale, String)
  DECL_ACCESSORS(icu_number_format, Managed<icu::NumberFormat>)
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
