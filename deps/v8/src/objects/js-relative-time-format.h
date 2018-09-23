// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_H_
#define V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_H_

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class RelativeDateTimeFormatter;
}

namespace v8 {
namespace internal {

class JSRelativeTimeFormat : public JSObject {
 public:
  // Initializes relative time format object with properties derived from input
  // locales and options.
  static MaybeHandle<JSRelativeTimeFormat> InitializeRelativeTimeFormat(
      Isolate* isolate,
      Handle<JSRelativeTimeFormat> relative_time_format_holder,
      Handle<Object> locales, Handle<Object> options);

  static Handle<JSObject> ResolvedOptions(
      Isolate* isolate, Handle<JSRelativeTimeFormat> format_holder);

  // Unpacks formatter object from corresponding JavaScript object.
  static icu::RelativeDateTimeFormatter* UnpackFormatter(
      Handle<JSRelativeTimeFormat> relative_time_format_holder);
  Handle<String> StyleAsString() const;
  Handle<String> NumericAsString() const;

  DECL_CAST(JSRelativeTimeFormat)

  // RelativeTimeFormat accessors.
  DECL_ACCESSORS(locale, String)

  DECL_ACCESSORS(formatter, Foreign)

  // Style: identifying the relative time format style used.
  //
  // ecma402/#sec-properties-of-intl-relativetimeformat-instances

  enum class Style {
    LONG,    // Everything spelled out.
    SHORT,   // Abbreviations used when possible.
    NARROW,  // Use the shortest possible form.
    COUNT
  };
  inline void set_style(Style style);
  inline Style style() const;

  // Numeric: identifying whether numerical descriptions are always used, or
  // used only when no more specific version is available (e.g., "1 day ago" vs
  // "yesterday").
  //
  // ecma402/#sec-properties-of-intl-relativetimeformat-instances
  enum class Numeric {
    ALWAYS,  // numerical descriptions are always used ("1 day ago")
    AUTO,    // numerical descriptions are used only when no more specific
             // version is available ("yesterday")
    COUNT
  };
  inline void set_numeric(Numeric numeric);
  inline Numeric numeric() const;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _) \
  V(StyleBits, Style, 2, _)    \
  V(NumericBits, Numeric, 1, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Style::LONG <= StyleBits::kMax);
  STATIC_ASSERT(Style::SHORT <= StyleBits::kMax);
  STATIC_ASSERT(Style::NARROW <= StyleBits::kMax);
  STATIC_ASSERT(Numeric::AUTO <= NumericBits::kMax);
  STATIC_ASSERT(Numeric::ALWAYS <= NumericBits::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  DECL_PRINTER(JSRelativeTimeFormat)
  DECL_VERIFIER(JSRelativeTimeFormat)

  // Layout description.
  static const int kJSRelativeTimeFormatOffset = JSObject::kHeaderSize;
  static const int kLocaleOffset = kJSRelativeTimeFormatOffset + kPointerSize;
  static const int kFormatterOffset = kLocaleOffset + kPointerSize;
  static const int kFlagsOffset = kFormatterOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

 private:
  static Style getStyle(const char* str);
  static Numeric getNumeric(const char* str);

  DISALLOW_IMPLICIT_CONSTRUCTORS(JSRelativeTimeFormat);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_H_
